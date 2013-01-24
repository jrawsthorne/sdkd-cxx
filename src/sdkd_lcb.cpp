#include "sdkd_internal.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <signal.h>

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

    unsigned portNumber;
    struct sockaddr_in listenAddr;

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

Program::Program(int argc, char **argv) :
        debugLevel(CBSDKD_LOGLVL_DEBUG),
        portFile(NULL),
        useColor(0),
        isPersistent(0),
        portNumber(0),
        initialTTL(0),
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

    SDKD_INIT_VIEWS();
    SDKD_INIT_WORKER_GLOBALS();

    signal(SIGPIPE, SIG_IGN);

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
    Program program = Program(argc, argv);
    program.run();
    return 0;
}
