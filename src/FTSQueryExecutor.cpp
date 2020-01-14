#include "sdkd_internal.h"
#include <thread>

namespace CBSdkd {
extern "C" {
lcb_MUTATION_TOKEN* mut;

static void
query_cb(lcb_INSTANCE *instance, int cbtype, const lcb_RESPSEARCH *resp) {
    void *cookie;
    lcb_respsearch_cookie(resp, &cookie);
    ResultSet *obj = reinterpret_cast<ResultSet *>(cookie);
    if (lcb_respsearch_is_final(resp)) {
        if (lcb_respsearch_status(resp) == LCB_SUCCESS && obj->fts_query_resp_count != 1) {
            fprintf(stderr, "FTS response does not match expected number of documents %d\n", obj->fts_query_resp_count);
        }
        fprintf(stderr, "Query completed with status code = %d\n", lcb_respsearch_status(resp));
        obj->setRescode(lcb_respsearch_status(resp) , true);
        return;
    }

    obj->fts_query_resp_count++;
}

static void cb_store(lcb_INSTANCE *instance, int cbType, const lcb_RESPBASE *resp) {
    const lcb_RESPSTORE *rb = (const lcb_RESPSTORE *)resp;
	if (lcb_respstore_status(rb) != LCB_SUCCESS)
	{
        fprintf(stderr, "FTS error while insert %d\n", lcb_respstore_status(rb));
	}
    lcb_respstore_mutation_token(rb, mut);
}
}


lcb_STATUS FTSQueryExecutor::runSearchOnPreloadedData(
		ResultSet& out,
		std::string &indexName,
		int kvCount)
{
    lcb_INSTANCE *instance = handle->getLcb();
    out.markBegin();
    out.fts_query_resp_count = 0;

    lcb_CMDSEARCH *ftscmd;
    lcb_cmdsearch_create(&ftscmd);
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

    lcb_cmdsearch_payload(ftscmd, query.c_str(), query.size());
    lcb_cmdsearch_callback(ftscmd, query_cb);

    lcb_STATUS err = lcb_search(instance, &out, ftscmd);
    lcb_cmdsearch_destroy(ftscmd);
    return err;
}

lcb_STATUS FTSQueryExecutor::runSearchUnderAtPlusConsistency(ResultSet &out,
		std::string &indexName)
{
    lcb_INSTANCE *instance = handle->getLcb();
    out.markBegin();
    out.fts_query_resp_count = 0;

    lcb_CMDSEARCH *ftscmd;
    lcb_cmdsearch_create(&ftscmd);
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

    lcb_install_callback(handle->getLcb(), LCB_CALLBACK_STORE, cb_store);

    lcb_sched_enter(handle->getLcb());

    lcb_CMDSTORE *cmd;
    lcb_STATUS err;
    lcb_cmdstore_create(&cmd, LCB_STORE_UPSERT);
    lcb_cmdstore_key(cmd, curk.data(), curk.size());
    lcb_cmdstore_value(cmd, curv.data(), curv.size());
    err = lcb_store(handle->getLcb(), NULL, cmd);
    lcb_cmdstore_destroy(cmd);
    if (err != LCB_SUCCESS)
    {
        return err;
    }

    lcb_sched_leave(handle->getLcb());
    lcb_wait(handle->getLcb(), LCB_WAIT_DEFAULT);

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

    lcb_cmdsearch_payload(ftscmd, query.c_str(), query.size());
    lcb_cmdsearch_callback(ftscmd, query_cb);

    generator++;
    err = lcb_search(instance, &out, ftscmd);
    lcb_cmdsearch_destroy(ftscmd);
    return err;
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
        lcb_STATUS err;
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
                    err, lcb_strerror_short(err));
        }
        lcb_wait(handle->getLcb(), LCB_WAIT_DEFAULT);
    }
    handle->externalLeave();
    return true;
}
}


