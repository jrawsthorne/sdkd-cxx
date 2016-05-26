#include "sdkd_internal.h"

namespace CBSdkd {
extern "C" {
static void
query_cb(lcb_t instance, int cbtype, const lcb_RESPFTS *resp) {
    ResultSet *obj = reinterpret_cast<ResultSet *>(resp->cookie);
    if ((resp->rflags & LCB_RESP_F_FINAL) && (resp->rc == LCB_SUCCESS)) {
        if (obj->fts_query_resp_count != obj->fts_limit) {
            fprintf(stderr, "FTS response does not match expected number of documents");
        }
        obj->setRescode(resp->rc , true);
        return;
    } else if (resp->rc == LCB_SUCCESS) {
        obj->fts_query_resp_count++;
    } else if (resp->rc != LCB_SUCCESS) {
        fprintf(stderr, "Got error on fts query callback 0x%x %s\n",
                    resp->rc, lcb_strerror(NULL, resp->rc));
    }
}
}

bool
FTSQueryExecutor::execute(ResultSet& out,
                          const ResultOptions& options,
                          const Request& req) {
    out.clear();

    handle->externalEnter();
    lcb_t instance = handle->getLcb();
    std::string indexName = req.payload[CBSDKD_MSGFLD_FTS_INDEXNAME].asString();
    out.fts_limit = req.payload[CBSDKD_MSGFLD_FTS_LIMIT].asInt64();

    while(!handle->isCancelled()) {
        out.markBegin();
        out.fts_query_resp_count = 0;
        lcb_CMDFTS ftscmd = { 0 };
        Json::Value queryJson;
        queryJson["indexName"] = indexName;
        queryJson["match"] = "FTS";
        queryJson["field"] = "type";
        queryJson["limit"] = limit;

        std::string query = Json::FastWriter().write(queryJson);

        ftscmd.query = query.c_str();
        ftscmd.nquery = query.size();
        ftscmd.callback = query_cb;

        lcb_error_t err = lcb_fts_query(instance, &out, &ftscmd);
        if (err != LCB_SUCCESS) {
            fprintf(stderr, "Erro scheduling fts query 0x%x %s\n",
                    err, lcb_strerror(NULL, err));
        }
        lcb_wait(handle->getLcb());
    }
    handle->externalLeave();
    return true;
}
}


