#include "sdkd_internal.h"

namespace CBSdkd {

N1QLCreateIndex::N1QLCreateIndex(Handle *handle) {
    this->handle = handle;
    this->success = false;
}

N1QLCreateIndex::~N1QLCreateIndex() {
}

void
N1QLCreateIndex::query_cb(lcb_t, int, const lcb_RESPN1QL *resp) {
    N1QLCreateIndex *obj = static_cast<N1QLCreateIndex *>(resp->cookie);
    if (resp->rc == LCB_SUCCESS) {
        obj->success = true;
    } else {
        obj->success = false;
    }
}

bool
N1QLCreateIndex::execute(Command cmd,
                            ResultSet& out,
                            const ResultOptions& options,
                            const Request& req)
{
    Json::Value ctlopts = req.payload[CBSDKD_MSGFLD_DSREQ_OPTS];
    string indexType = ctlopts[CBSDKD_MSDGFLD_NQ_PARAM].asString();
    string indexEngine = ctlopts[CBSDKD_MSDGFLD_NQ_PARAMVALUES].asString();
    string indexName = ctlopts[CBSDKD_MSDFLD_NQ_DEFAULT_INDEX_NAME].asString();
    string query;
    char buf[1024];
    memset(buf, '\0', sizeof(buf));
    if(sprintf(buf, "CREATE PRIMARY INDEX ON `%s` using %s",
                                                this->handle->options.bucket.c_str(),
                                                indexEngine.c_str()) != -1) {
        return false;
    }
    query = std::string(buf);

    lcb_CMDN1QL qcmd = {0};
    qcmd.callback = query_cb;

    lcb_N1QLPARAMS *n1p = lcb_n1p_new();
    lcb_error_t err;
    err = lcb_n1p_setquery(n1p, query.c_str(), query.length(),
            LCB_N1P_QUERY_STATEMENT);

    if (err != LCB_SUCCESS) return false;

    err = lcb_n1p_mkcmd(n1p, &qcmd);
    if (err != LCB_SUCCESS) return false;

    err = lcb_n1ql_query(handle->getLcb(), this, &qcmd);
    if (err != LCB_SUCCESS) return false;

    err = lcb_n1p_mkcmd(n1p, &qcmd);
    if (err != LCB_SUCCESS) return false;

    lcb_wait(handle->getLcb());
    lcb_n1p_free(n1p);
    return this->success;
}
}
