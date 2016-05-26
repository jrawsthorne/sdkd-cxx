#ifndef SDKD_SUBDOC_H_
#define SDKD_SUBDOC_H_
#endif

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#include "sdkd_internal.h"

namespace CBSdkd {
using namespace std;

class FTSLoader : protected DebugContext {
public:
    FTSLoader(Handle* handle) {
        this->handle = handle;
    }
    virtual ~FTSLoader() {}

    bool populate(const Dataset& ds);

private:
    Handle* handle;
};

class FTSQueryExecutor : protected DebugContext {
public:
    FTSQueryExecutor(Handle* handle) {
        this->handle = handle;
    }
    virtual ~FTSQueryExecutor() {}

    bool execute(ResultSet& rs, 
            const ResultOptions& options, 
            const Request &req);

    void setLimit(int limit) {
        this->limit = limit;
    };

    int getLimit() {
        return this->limit;
    };

private:
    Handle* handle;
    int limit;
};
}


