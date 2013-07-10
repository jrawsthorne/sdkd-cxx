#include "sdkd_internal.h"
#include <iostream>

using namespace CBSdkd;
using namespace std;

#ifdef _WIN32
typedef lcb_error_t (*plugin_creator_func)(int,lcb_io_opt_t*,void*);
static plugin_creator_func plugin_creator;
extern "C" {
static int init_io_plugin(const char *name, const char *symbol)
{
    HMODULE hPlugin;

    char creator_buf[4096] = { 0 };

    if (!name) {
        return 0;
    }

    hPlugin = LoadLibrary(name);
    if (!hPlugin) {
        fprintf(stderr, "Couldn't load %s. %d\n", name, GetLastError());
        abort();
    }

    if (symbol == NULL) {
        sprintf(creator_buf, "lcb_create_%_io_iopts", name);
        symbol = creator_buf;
    }

    plugin_creator = (plugin_creator_func) GetProcAddress(hPlugin, symbol);
    if (!plugin_creator) {
        fprintf(stderr, "Couldn't load symbol '%s'. [%d]\n",
                symbol, GetLastError());
        abort();
    }
    return 0;
}

lcb_io_opt_t sdkd_create_iops(void)
{
    lcb_io_opt_t ret;
    lcb_error_t err;

    if (!plugin_creator) {
        return NULL;
    }

    err = plugin_creator(0, &ret, NULL);
    if (err != LCB_SUCCESS) {
        fprintf(stderr, "Couldn't init iops: %d\n", err);
    }
    return ret;
}
}
#else

static int init_io_plugin(const char *name, const char *symbol)
{
    char symbuf[4096];
    if (!name) {
        return 0;
    }
    if (!symbol) {
        sprintf(symbuf, "lcb_create_%s_iops", name);
    }
    setenv("LIBCOUCHBASE_EVENT_PLUGIN_NAME", name, 1);
    setenv("LIBCOUCHBASE_EVENT_PLUGIN_SYMBOL", symbol, 1);
    return 0;
}

lcb_io_opt_t sdkd_create_iops(void)
{
    return NULL;
}
#endif


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
    init_io_plugin(myOptions.ioPluginName, myOptions.ioPluginSymbol);
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
