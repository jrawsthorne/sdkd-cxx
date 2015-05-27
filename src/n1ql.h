#ifndef SDKD_N1QL_H
#define SDKD_N1QL_H
#endif

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#include "sdkd_internal.h"

namespace CBSdkd {

class N1QLQueryExecutor : protected DebugContext {
public:
    N1QLQueryExecutor(Handle *handle);
    ~N1QLQueryExecutor();
    bool execute(Command cmd,
            ResultSet& s,
            const ResultOptions& opts,
            const Request& req);

private:
    Handle *handle;
};

class N1QLCreateIndex : protected DebugContext {
public:
    N1QLCreateIndex(Handle *handle);
    ~N1QLCreateIndex();
    static void query_cb(lcb_t, int, const lcb_RESPN1QL*);
    bool execute(Command cmd,
            ResultSet& s,
            const ResultOptions& opts,
            const Request& req);
private:
    Handle *handle;
    bool success;
};
}
