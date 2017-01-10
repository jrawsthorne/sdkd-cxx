#include "sdkd_internal.h"

namespace CBSdkd {
extern "C" {
static void
query_cb(lcb_t instance, int cbtype, const lcb_RESPFTS *resp) {
    ResultSet *obj = reinterpret_cast<ResultSet *>(resp->cookie);
    if ((resp->rflags & LCB_RESP_F_FINAL) && (resp->rc == LCB_SUCCESS)) {
        if (obj->fts_query_resp_count != 1) {
            fprintf(stderr, "FTS response does not match expected number of documents %d\n", obj->fts_query_resp_count);
        }
        obj->setRescode(resp->rc , true);
        return;
    }

    obj->fts_query_resp_count++;
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
    unsigned int kvCount = req.payload[CBSDKD_MSGFLD_FTS_COUNT].asInt64();
    int generator = 0;
    while(!handle->isCancelled()) {
        out.markBegin();
        out.fts_query_resp_count = 0;
        lcb_CMDFTS ftscmd = { 0 };

        generator = (generator + 1) % (2 * kvCount);

        Json::Value queryJson;
        Json::Value matchJson;
        queryJson["indexName"] = indexName;
        std::string searchField = generator % 2 == 0 ?
            "SampleValue" + std::to_string(generator / 2) :
            "SampleSubvalue" + std::to_string(generator / 2);
        matchJson["match"] = searchField;
        queryJson["query"] = matchJson;

        std::string query = Json::FastWriter().write(queryJson);

        ftscmd.query = query.c_str();
        ftscmd.nquery = query.size();
        ftscmd.callback = query_cb;

        lcb_error_t err = lcb_fts_query(instance, &out, &ftscmd);
        if (err != LCB_SUCCESS) {
            fprintf(stderr, "Error scheduling fts query 0x%x %s\n",
                    err, lcb_strerror(NULL, err));
        }
        lcb_wait(handle->getLcb());
    }
    handle->externalLeave();
    return true;
}
}


