#include "sdkd_internal.h"

namespace CBSdkd {

struct DaemonOptions {
    DaemonOptions() {
        memset(this, 0, sizeof(*this));
    }

    int debugLevel;
    int debugColors;
    char *portFile;

    // Don't exit after first CBSDK session
    int isPersistent;
    int initialTTL;
    unsigned portNumber;

    // IO Plugin name/symbol to pass to libcouchbase
    char *ioPluginName;
    char *ioPluginSymbol;
    // A short name. Expect to find a 'libcouchbase_<this>' and a
    // lcb_create_<this>_opts
    char *ioPluginBase;

    // Configuration cache
    char *conncachePath;

    // Re-create lcb_t after each operation
    int noPersist;
};

class Daemon {

public:
    Daemon(const DaemonOptions& userOptions);
    virtual ~Daemon();

    void runServer();
    void writePortInfo();
    void prepareAddress();
    void run();

    const DaemonOptions& getOptions() const {
        return myOptions;
    }

    lcb_io_opt_t createIO();

    static Daemon* MainDaemon;

private:
    DaemonOptions myOptions;
    struct sockaddr_in listenAddr;
    FILE *infoFp;
    void initDebugSettings();
    bool initIOPS();
    void processIoOptions();
    bool verifyIoPlugin();

    bool hasCreateOptions;
    lcb_create_io_ops_st ioCreationOptions;

    std::string s_ioSymbol;
    std::string s_ioDLL;
};

}
