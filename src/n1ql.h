#ifndef SDKD_N1QL_H
#define SDKD_N1QL_H
#endif

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#include "sdkd_internal.h"

namespace CBSdkd {

class N1QLConfig {
public:
    string const& get_index_name() const {
        return this->indexName;
    }
    void set_index_name(string const& name) {
        this->indexName = name;
    }
    string const& get_params() const {
        return this->params;
    }
    void set_params(string const& params) {
        this->params = params;
    }
    string const& get_param_values() const {
        return this->paramValues;
    }
    void set_param_values(string const& paramValues) {
        this->paramValues = paramValues;
    }
private:
    string indexName;
    string params;
    string paramValues;
};

class N1QL : protected DebugContext {
public:
    N1QL(Handle *handle) :
        handle(handle) {
    }
    ~N1QL() {}
    bool query(char *buf, lcb_CMDN1QL *qcmd, int type, void *cookie) {
        lcb_N1QLPARAMS *n1p = lcb_n1p_new();
        lcb_error_t err;

        string query = std::string(buf);
        err = lcb_n1p_setquery(n1p, query.c_str(), query.length(),
                type);

        if (err != LCB_SUCCESS) return false;

        err = lcb_n1p_mkcmd(n1p, qcmd);
        if (err != LCB_SUCCESS) return false;

        err = lcb_n1ql_query(handle->getLcb(), cookie, qcmd);
        if (err != LCB_SUCCESS) return false;


        lcb_wait(handle->getLcb());
        lcb_n1p_free(n1p);
        return true;
    };
    bool prepareQuery(char *buf, lcb_CMDN1QL *qcmd, int type, void *cookie) {
    }

private:
    Handle *handle;
};

class N1QLQueryExecutor : public N1QL {
public:
    N1QLQueryExecutor(Handle *handle) :
        N1QL(handle), handle(handle) {
    }
    ~N1QLQueryExecutor();
    bool execute(Command cmd,
            ResultSet& s,
            const ResultOptions& opts,
            const Request& req,
            N1QLConfig *config);
private:
    static void query_cb(lcb_t, int, const lcb_RESPN1QL*);
    Handle *handle;
};

class N1QLCreateIndex : public N1QL {
public:
    N1QLCreateIndex(Handle *handle) :
        N1QL(handle), success(false), handle(handle) {
    }
    ~N1QLCreateIndex();
    bool execute(Command cmd, const Request& req, N1QLConfig *config);
private:
    bool success;
    Handle *handle;
    static void query_cb(lcb_t, int, const lcb_RESPN1QL*);

};
}
