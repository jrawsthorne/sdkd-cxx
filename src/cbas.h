#ifndef SDKD_SUBDOC_H_
#define SDKD_SUBDOC_H_
#endif

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#include "sdkd_internal.h"

namespace CBSdkd {
    using namespace std;

    class CBASLoader : protected DebugContext {
    public:
        CBASLoader(Handle* handle) {
            this->handle = handle;
        }
        virtual ~CBASLoader() {}

        bool populate(const Dataset& ds);

    private:
        Handle* handle;
    };

    class CBASQueryExecutor : protected DebugContext {
    public:
        CBASQueryExecutor(Handle* handle) {
            this->handle = handle;
            generator = 0;
        }
        virtual ~CBASQueryExecutor() {}

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
        int generator;

    };
}


