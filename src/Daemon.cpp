#include "sdkd_internal.h"
#include <iostream>
#include <algorithm>

using namespace CBSdkd;
using namespace std;

#ifdef _WIN32
typedef lcb_error_t (*plugin_creator_func)(int,lcb_io_opt_t*,void*);

bool
Daemon::initIOPS()
{
    HMODULE hPlugin;
    hPlugin = LoadLibrary(s_ioDLL.c_str());

    if (!hPlugin) {
        fprintf(stderr, "Couldn't load %s. %d\n", s_ioDLL.c_str(), GetLastError());
        abort();
    }

    ioCreationOptions.version = 2;

    plugin_creator_func plugin_creator =
            (plugin_creator_func) GetProcAddress(hPlugin, s_ioSymbol.c_str());

    if (!plugin_creator) {
        fprintf(stderr,
                "Couldn't load symbol '%s'. [%d]\n",
                s_ioSymbol.c_str(),
                GetLastError());
        abort();
    }

    ioCreationOptions.v.v2.create = plugin_creator;
    return true;
}
#else

bool
Daemon::initIOPS()
{
    ioCreationOptions.version = 1;
    ioCreationOptions.v.v1.sofile = s_ioDLL.c_str();
    ioCreationOptions.v.v1.symbol = s_ioSymbol.c_str();
    ioCreationOptions.v.v1.cookie = NULL;
    return true;
}
#endif

#ifdef _WIN32
#define LIBSUFFIX "dll"
#elif __APPLE__
#define LIBSUFFIX "dylib"
#else
#define LIBSUFFIX "so"
#endif

void
Daemon::processIoOptions()
{
    using namespace std;

    if (myOptions.ioPluginName == NULL && myOptions.ioPluginBase == NULL) {
        hasCreateOptions = false;
        return;
    }

    hasCreateOptions = true;

    if (myOptions.ioPluginBase) {
        lcb_io_ops_type_t iotype;

        if (*myOptions.ioPluginBase == '@') {
            string iot_builtin = string(myOptions.ioPluginBase + 1);

            transform(iot_builtin.begin(),
                      iot_builtin.end(),
                      iot_builtin.begin(),
                      ::tolower);

            if (iot_builtin.find("libevent") != string::npos) {
                iotype = LCB_IO_OPS_LIBEVENT;
            } else if (iot_builtin.find("libev") != string::npos) {
                iotype = LCB_IO_OPS_LIBEV;
#if LCB_VERSION >= 0x020007
            } else if (iot_builtin.find("select") != string::npos) {
                iotype = LCB_IO_OPS_SELECT;
#endif

#if LCB_VERSION >= 0x020100 || defined(SDKD_LCB_HEAD)
            } else if (iot_builtin.find("iocp") != string::npos) {
                iotype = LCB_IO_OPS_WINIOCP;
            } else if (iot_builtin.find("libuvdl") != string::npos) {
                iotype = LCB_IO_OPS_LIBUVDL;
            } else if (iot_builtin.find("libuvld") != string::npos) {
                iotype = LCB_IO_OPS_LIBUVLD;
#endif
            } else if (iot_builtin.find("default") != string::npos) {
                iotype = LCB_IO_OPS_DEFAULT;
            } else {
                fprintf(stderr,
                        "Could not find matching io type for '%s'\n",
                        iot_builtin.c_str());
                exit(1);
            }

            ioCreationOptions.version = 0;
            ioCreationOptions.v.v0.type = iotype;
            ioCreationOptions.v.v0.cookie = NULL;

            return;
        }

        // Ignore everything else
        s_ioSymbol = "lcb_create_";
        s_ioSymbol += myOptions.ioPluginBase;
        s_ioSymbol += "_io_opts";

        s_ioDLL = "libcouchbase_";
        s_ioDLL += myOptions.ioPluginBase;
        s_ioDLL += ".";
        s_ioDLL += LIBSUFFIX;

    } else {

        if (!myOptions.ioPluginSymbol) {
            fprintf(stderr, "plugin-symbol must be provided with plugin-name");
            exit(1);
        }

        s_ioSymbol = std::string(myOptions.ioPluginSymbol);
    }

    initIOPS();
}

Daemon::Daemon(const DaemonOptions& userOptions)
: myOptions(userOptions),
  infoFp(NULL)

{
    if (myOptions.portFile == NULL && myOptions.portNumber == 0) {
        cerr << "Must specify a port file or port number" << endl;
        exit(1);
    }

    if (myOptions.portFile) {
        infoFp = fopen(myOptions.portFile, "w");
        if (!infoFp) {
            perror(myOptions.portFile);
            exit(0);
        }
    }

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif


    initDebugSettings();
    processIoOptions();


    SDKD_INIT_VIEWS();
    SDKD_INIT_WORKER_GLOBALS();

    prepareAddress();

    sdkd_init_timer();
    sdkd_set_ttl(myOptions.initialTTL);

}

Daemon::~Daemon()
{
    if (myOptions.portFile != NULL) {
        remove(myOptions.portFile);
    }
}

void
Daemon::prepareAddress()
{
    memset(&listenAddr, 0, sizeof(listenAddr));
    listenAddr.sin_port = htons(myOptions.portNumber);
    listenAddr.sin_family = AF_INET;
}

void
Daemon::runServer()
{
    MainDispatch server = MainDispatch();

    if (!server.establishSocket(&listenAddr)) {
        perror("Establishing listening socket..");
        exit(1);
    }

    log_noctx_info("Listening on port %d", ntohs(listenAddr.sin_port));
    writePortInfo();
    server.run();
}

void
Daemon::writePortInfo()
{
    if (!infoFp) {
        return;
    }

    fprintf(infoFp, "%d\n", ntohs(listenAddr.sin_port));
    fclose(infoFp);
    infoFp = NULL;
}

void
Daemon::initDebugSettings() {
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.level =
            (cbsdkd_loglevel_t)myOptions.debugLevel;

    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.color =
            myOptions.debugColors;

    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.initialized = 1;
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.out = stderr;
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.prefix = "cbskd-test";
}

void
Daemon::run()
{
    do {
        runServer();
    } while (myOptions.isPersistent);
}

lcb_io_opt_t
Daemon::createIO()
{
    if (!hasCreateOptions) {
        return NULL;
    }

    lcb_error_t err;
    lcb_io_opt_t ret;

    err = lcb_create_io_ops(&ret, &ioCreationOptions);
    if (err != LCB_SUCCESS) {
        fprintf(stderr, "Got error %d when trying to create IO. Abort\n", err);
        abort();
    }
    return ret;
}
