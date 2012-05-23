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

#include "IODispatch.h"

using namespace std;
using namespace CBSdkd;

DebugContext CBSdkd::CBsdkd_Global_Debug_Context;


int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [option=value...]\n", argv[0]);
        cerr << "infofile=FILE [ specify this file to exchange port information\n";
        cerr << "debug=1 [ enable debug output ]\n";
        exit(1);
    }

    std::string kvpair;
    std::map<std::string,std::string> opt_pairs;

    for (int ii = 1; ii < argc; ii++) {
        istringstream iss(argv[ii]);

        while(getline(iss, kvpair, ',')) {
            opt_pairs[kvpair.substr(0, kvpair.find_first_of('='))] =
                    kvpair.substr(kvpair.find_first_of('=')+1);
        }
    }

    const char *fname;
    if (!opt_pairs["infofile"].size()) {
        cerr << "Must have infofile!\n";
        exit(1);
    }

    fname = opt_pairs["infofile"].c_str();

    FILE *infofp = fopen(fname, "w");
    if (!infofp) {
        perror(fname);
        exit(1);
    }

    if (opt_pairs["debug"].size()) {
        cbsdkd_Default_Log_Level = CBSDKD_LOGLVL_DEBUG;
    }
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.color = 1;
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.initialized = 1;
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.out = stderr;
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.prefix = "cbskd-test";

    if (opt_pairs["nocolor"].size()) {
        CBsdkd_Global_Debug_Context.cbsdkd__debugctx.color = 0;
    }

    MainDispatch server = MainDispatch();
    struct sockaddr_in saddr = { 0 };
    if (!server.establishSocket(&saddr)) {
        perror("Establishing listening socket..");
        exit(1);
    }

    log_noctx_info("Listening on port %d", ntohs(saddr.sin_port));

    fprintf(infofp, "%d\n", ntohs(saddr.sin_port));
    fclose(infofp);

    signal(SIGPIPE, SIG_IGN);
    server.run();

    return 0;
}
