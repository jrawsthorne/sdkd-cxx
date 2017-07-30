#include "sdkd_internal.h"
#include <thread>

namespace CBSdkd {
extern "C" {
static const lcb_MUTATION_TOKEN* mut;

static void
query_cb(lcb_t instance, int cbtype, const lcb_RESPFTS *resp) {
    ResultSet *obj = reinterpret_cast<ResultSet *>(resp->cookie);
    if ((resp->rflags & LCB_RESP_F_FINAL)) {
        if (resp->rc == LCB_SUCCESS && obj->fts_query_resp_count != 1) {
            fprintf(stderr, "FTS response does not match expected number of documents %d\n", obj->fts_query_resp_count);
        }
        fprintf(stderr, "Query completed with status code = %d\n", resp->rc);
        obj->setRescode(resp->rc , true);
        return;
    }

    obj->fts_query_resp_count++;
}

static void cb_store(lcb_t instance, int cbType, const lcb_RESPBASE *resp) {
	if (resp->rc != LCB_SUCCESS)
	{
        fprintf(stderr, "FTS error while insert %d\n", resp->rc);
	}
    mut = lcb_resp_get_mutation_token(cbType, resp);
}
}


lcb_error_t FTSQueryExecutor::runSearchOnPreloadedData(
		ResultSet& out,
		std::string &indexName,
		int kvCount)
{
    lcb_t instance = handle->getLcb();
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

    return lcb_fts_query(instance, &out, &ftscmd);
}

lcb_error_t FTSQueryExecutor::runSearchUnderAtPlusConsistency(ResultSet &out,
		std::string &indexName)
{
    lcb_t instance = handle->getLcb();
    out.markBegin();
    out.fts_query_resp_count = 0;

    lcb_CMDFTS ftscmd = { 0 };
    std::stringstream sv;
    sv << "SampleValue:" << std::this_thread::get_id() << ":" << std::to_string(generator);
    std::string match_term = sv.str();

    Json::Value doc;
    doc["value"] = match_term;
    std::string curv = Json::FastWriter().write(doc);
    std::stringstream ss;
    ss << "key" << std::this_thread::get_id() << ":";
    std::string curk = ss.str();
    if (generator % 2 == 0)
        curk += std::to_string(generator);
    else
        curk += std::to_string(generator/2);

    lcb_install_callback3(handle->getLcb(), LCB_CALLBACK_STORE, cb_store);

    lcb_sched_enter(handle->getLcb());

    lcb_CMDSTORE cmd = { 0 };
    cmd.operation = LCB_UPSERT;
    cmd.value.vtype = LCB_KV_COPY;

    LCB_CMD_SET_KEY(&cmd, curk.data(), curk.size());
    cmd.value.u_buf.contig.bytes = curv.data();
    cmd.value.u_buf.contig.nbytes = curv.size();
    lcb_error_t err = lcb_store3(handle->getLcb(), NULL, &cmd);
    if (err != LCB_SUCCESS)
    {
        return err;
    }

    lcb_sched_leave(handle->getLcb());
    lcb_wait(handle->getLcb());

    Json::Value queryJson;
    Json::Value matchJson;
    queryJson["indexName"] = indexName;
    std::string searchField = match_term;
    matchJson["match"] = searchField;
    queryJson["query"] = matchJson;

    Json::Value mutTokJson;
    Json::Value vectorFtsIndexJson;
    Json::Value levelJson;
    Json::Value consistencyJson;

    char tokenValue[128];
    char tokenKey[128];
    sprintf(tokenKey, "%u/%llu", mut->vbid_, mut->uuid_);
    sprintf(tokenValue, "%llu", mut->seqno_);
    mutTokJson[tokenKey] = tokenValue;
    vectorFtsIndexJson[indexName] = mutTokJson;
    levelJson["level"] = "at_plus";
    levelJson["vectors"] = vectorFtsIndexJson;
    consistencyJson["consistency"] = levelJson;
    queryJson["ctl"] = consistencyJson;

    std::string query = Json::FastWriter().write(queryJson);

    ftscmd.query = query.c_str();
    ftscmd.nquery = query.size();
    ftscmd.callback = query_cb;

    generator++;
    return lcb_fts_query(instance, &out, &ftscmd);
}

bool
FTSQueryExecutor::execute(ResultSet& out,
                          const ResultOptions& options,
                          const Request& req) {
    out.clear();

    handle->externalEnter();
    std::string indexName = req.payload[CBSDKD_MSGFLD_FTS_INDEXNAME].asString();
    unsigned int kvCount = req.payload[CBSDKD_MSGFLD_FTS_COUNT].asInt64();

    // Check consistency level of fts query
    bool not_bounded = true;
    if (strcmp(req.payload[CBSDKD_MSGFLD_FTS_CONSISTENCY].asCString(), "at_plus") == 0)
        not_bounded = false;
    log_info("running fts queries with not_bounded = %d", not_bounded);

    while(!handle->isCancelled()) {
        lcb_error_t err;
        if (not_bounded)
        {
            err = runSearchOnPreloadedData(out, indexName, kvCount);
        }
        else
        {
            err = runSearchUnderAtPlusConsistency(out, indexName);
        }

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


