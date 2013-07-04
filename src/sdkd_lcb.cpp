#include "sdkd_internal.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <map>

#include "cliopts.h"

using namespace std;
using namespace CBSdkd;

DebugContext CBSdkd::CBsdkd_Global_Debug_Context;

class Program {
public:
    Program(int argc, char **argv);
    virtual ~Program() {}
    void runServer();
    void writePortInfo();
    void prepareAddress();
    void run();

    int debugLevel;
    char *portFile;
    int useColor;
    int isPersistent;
    int initialTTL;
    int printVersion;

    unsigned portNumber;
    struct sockaddr_in listenAddr;
    char *ioPluginName;
    char *ioPluginSymbol;

private:
    FILE *infoFp;

    bool parseLegacyArgs(int argc, char **argv);
    bool parseCliOptions(int argc, char **argv);
    void initDebugSettings(void);
};

bool
Program::parseLegacyArgs(int argc, char **argv)
{
    std::string kvpair;
    std::map<std::string,std::string> opt_pairs;

    for (int ii = 1; ii < argc; ii++) {
        istringstream iss(argv[ii]);

        while(getline(iss, kvpair, ',')) {
            opt_pairs[kvpair.substr(0, kvpair.find_first_of('='))] =
                    kvpair.substr(kvpair.find_first_of('=')+1);
        }
    }

    if (!opt_pairs["infofile"].size()) {
        return false;
    }

    portFile = strdup(opt_pairs["infofile"].c_str());

    if (opt_pairs["debug"].size()) {
        debugLevel = CBSDKD_LOGLVL_DEBUG;
    }
    return true;
}

bool
Program::parseCliOptions(int argc, char **argv)
{
    cliopts_entry entries[] = {
            { 'd', "debug", CLIOPTS_ARGT_INT, &debugLevel,
                    "Level (0=off, 1=most verbose, higher numbers less verbsose)",
                    "LEVEL"
            },

            { 'c', "color", CLIOPTS_ARGT_NONE, &useColor,
                    "Whether to use color logging" },

            { 'f', "portfile", CLIOPTS_ARGT_STRING, &portFile,
                    "File to write port information (if listening on random port)",
                    "FILE"
            },

            { 'l', "listen", CLIOPTS_ARGT_UINT, &portNumber,
                    "Port to listen on",
                    "PORT"
            },

            { 'P', "persist", CLIOPTS_ARGT_NONE, &isPersistent,
                    "Keep running after GOODBYEs",
            },

            { 0, "ttl", CLIOPTS_ARGT_INT, &initialTTL,
                    "TTL For daemon"
            },

            { 'V', "version", CLIOPTS_ARGT_NONE, &printVersion,
                    "Print versions and exit"
            },

            { 0, "conncache", CLIOPTS_ARGT_STRING, &SDKD_Conncache_Path,
                    "Path to cached configuration"
            },

            { 0, "no-persist", CLIOPTS_ARGT_NONE, &SDKD_No_Persist,
                    "Reset LCB Handle after each operation"
            },

            { 0, "io-plugin-name", CLIOPTS_ARGT_STRING, &ioPluginName,
                    "Name of IO Plugin to use. Must be in linker search path"
            },

            { 0, "io-plugin-symbol", CLIOPTS_ARGT_STRING, &ioPluginSymbol,
                    "Symbol within the IO plugin which contains the initializer"
            },

            { 0 }
    };

    int last_arg;
    cliopts_parse_options(entries, argc, argv, &last_arg, NULL);
    return true;

}

void
Program::initDebugSettings()
{
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.level = (cbsdkd_loglevel_t)debugLevel;
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.color = useColor;
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.initialized = 1;
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.out = stderr;
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.prefix = "cbskd-test";

}

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

Program::Program(int argc, char **argv) :
        debugLevel(CBSDKD_LOGLVL_DEBUG),
        portFile(NULL),
        useColor(0),
        isPersistent(0),
        portNumber(0),
        initialTTL(0),
        printVersion(0),
        ioPluginName(NULL),
        ioPluginSymbol(NULL),
        infoFp(NULL)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [option=value...]\n", argv[0]);
        cerr << "infofile=FILE [ specify this file to exchange port information\n";
        cerr << "debug=1 [ enable debug output ]\n";
        cerr << "Extended options available (use --help)" << endl;
        exit(1);
    }

    parseLegacyArgs(argc, argv) || parseCliOptions(argc, argv);

    if (printVersion) {
        Json::Value value;
        Handle::VersionInfoJson(value);
        cout << value.toStyledString() << endl;
        exit(0);
    }
    init_io_plugin(ioPluginName, ioPluginSymbol);

    SDKD_INIT_VIEWS();
    SDKD_INIT_WORKER_GLOBALS();

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    if (portFile == NULL && portNumber == 0) {
        cerr << "Must specify a port file or port number" << endl;
        exit(1);
    }

    if (portFile) {
        infoFp = fopen(portFile, "w");
        if (!infoFp) {
            perror(portFile);
            exit(0);
        }
    }

    initDebugSettings();
    prepareAddress();
    sdkd_init_timer();
    sdkd_set_ttl(initialTTL);
}

void Program::run()
{
    do {
        runServer();
    } while (isPersistent);

}

void
Program::prepareAddress()
{
    memset(&listenAddr, 0, sizeof(listenAddr));
    listenAddr.sin_port = htons(portNumber);
    listenAddr.sin_family = AF_INET;
}

void
Program::writePortInfo()
{
    if (!infoFp) {
        return;
    }
    fprintf(infoFp, "%d\n", ntohs(listenAddr.sin_port));
    fclose(infoFp);
    infoFp = NULL;
}

void
Program::runServer()
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

int
main(int argc, char **argv)
{
#ifdef _WIN32
    // Initialize winsock
    WORD wVersionRequested;
    WSADATA wsaData;
    int rv;
    wVersionRequested = MAKEWORD(2, 2);
    rv = WSAStartup(wVersionRequested, &wsaData);
    assert(rv == 0);
#endif

    Program program = Program(argc, argv);
    program.run();

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
