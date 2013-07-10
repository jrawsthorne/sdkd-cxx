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

    static Daemon* MainDaemon;

private:
    DaemonOptions myOptions;
    struct sockaddr_in listenAddr;
    FILE *infoFp;
    void initDebugSettings(void);
};

}
