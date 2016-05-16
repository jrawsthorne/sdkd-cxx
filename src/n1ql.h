#ifndef SDKD_N1QL_H
#define SDKD_N1QL_H
#endif

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#include "sdkd_internal.h"

namespace CBSdkd {

class N1QL : protected DebugContext {
public:
    N1QL(Handle *handle) :
        handle(handle) {
    }
    ~N1QL() {}

    bool query(const char *buf, lcb_CMDN1QL *qcmd, int type, void *cookie, lcb_error_t& err, std::string& consistency, Json::Value& scan_vector) {

        Json::Value req;
        req["statement"] = std::string(buf);
        req["scan_consistency"] = consistency;
        if (consistency == "at_plus") {
            req["scan_vectors"] = scan_vector;

        }
        std::string q = Json::FastWriter().write(req);
        qcmd->query = q.c_str();
        qcmd->nquery = q.size();
        qcmd->content_type = "application/json";

        err = lcb_n1ql_query(handle->getLcb(), cookie, qcmd);
        if (err != LCB_SUCCESS) return false;
        return true;
    };

    void split(const std::string &s, char delim, std::vector<std::string> &elems) {
        std::stringstream ss(s);
        std::string item;

        while(std::getline(ss, item, delim)) {
            if (!item.empty()) {
                elems.push_back(item);
            }
        }
    };

    bool is_qsuccess;
    lcb_error_t rc;
private:
    Handle *handle;
};

class N1QLQueryExecutor : public N1QL {
public:
    N1QLQueryExecutor(Handle *handle) :
        N1QL(handle), is_isuccess(false),
        insert_err(LCB_SUCCESS), handle(handle) {
    }
    ~N1QLQueryExecutor() {}

    bool execute(Command cmd,
            ResultSet& s,
            const ResultOptions& opts,
            const Request& req);

    bool is_isuccess;
    bool query;
    lcb_error_t insert_err;
    Json::Value tokens;
private:
    bool insertDoc(lcb_t instance,
            std::vector<std::string>& params,
            std::vector<std::string>& paramValues,
            lcb_error_t& err);

    Handle *handle;
    void split(const std::string& str, char delim, std::vector<std::string>& elems);
};

class N1QLCreateIndex : public N1QL {
public:
    N1QLCreateIndex(Handle *handle) :
        N1QL(handle), handle(handle) {
    }
    ~N1QLCreateIndex() {}
    bool execute(Command cmd, const Request& req);

private:
    Handle *handle;
};

class N1QLLoader : public N1QL {
public:
    N1QLLoader(Handle *handle) :
        N1QL(handle), handle(handle) {
    }
    bool populate(const Dataset& ds);
private:
    Handle *handle;

};
}
