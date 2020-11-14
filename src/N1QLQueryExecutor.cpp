#include "sdkd_internal.h"


namespace CBSdkd {
extern "C" {
static void
insert_cb(lcb_INSTANCE *instance, int type, lcb_RESPBASE *resp) {
    const lcb_RESPSTORE *sresp = (lcb_RESPSTORE *)resp;
    void *cookie;
    lcb_respstore_cookie(sresp, &cookie);
    N1QLQueryExecutor *obj = reinterpret_cast<N1QLQueryExecutor*>(cookie);
    if (lcb_respstore_status(sresp) == LCB_SUCCESS) {
        obj->is_isuccess = true;
        lcb_MUTATION_TOKEN *ss = NULL;
        lcb_respstore_mutation_token(sresp, ss);
        Json::Value vbucket;
        Json::Value mtInfoArr = Json::Value(Json::arrayValue);
        obj->tokens = vbucket;
    }
    obj->insert_err = lcb_respstore_status(sresp);
}
}

bool
N1QLQueryExecutor::insertDoc(lcb_INSTANCE *instance,
        std::vector<std::string> &params,
        std::vector<std::string> &paramValues,
        lcb_STATUS& err) {

    std::vector<std::string>::iterator pit = params.begin();
    std::vector<std::string>::iterator vit = paramValues.begin();

    Json::Value doc;
    doc["id"] = std::to_string(handle->hid);

    for(;pit<params.end(); pit++, vit++) {
        doc[*pit] = *vit;
    }

    std::string val = Json::FastWriter().write(doc);
    std::string key = doc["id"].asString();
    pair<string, string> collection = handle->getCollection(key);

    lcb_install_callback(instance, LCB_CALLBACK_STORE, (lcb_RESPCALLBACK)insert_cb);
    lcb_CMDSTORE *scmd;

    lcb_cmdstore_create(&scmd, LCB_STORE_UPSERT);
    lcb_cmdstore_collection(scmd, collection.first.c_str(), collection.first.size(), collection.second.c_str(), collection.second.size());
    lcb_cmdstore_key(scmd, key.c_str(), key.size());
    lcb_cmdstore_value(scmd, val.c_str(), val.size());

    err = lcb_store(instance, (void *)this, scmd);
    lcb_cmdstore_destroy(scmd);
    if (err != LCB_SUCCESS) {
        this->insert_err = err;
        return false;
    }

    lcb_wait(instance, LCB_WAIT_DEFAULT);
    err = this->insert_err;
    if (err != LCB_SUCCESS) {
        return false;
    }
    return true;
}

extern "C" {
static void
dump_http_error(const lcb_RESPQUERY *resp) {
    const lcb_QUERY_ERROR_CONTEXT *ctx;
    lcb_respquery_error_context(resp, &ctx);

    const char *endpoint = nullptr;
    size_t endpoint_len = 0;
    lcb_errctx_query_endpoint(ctx, &endpoint, &endpoint_len);

    uint32_t http_code = 0;
    lcb_errctx_query_http_response_code(ctx, &http_code);

    uint32_t err_code = 0;
    lcb_errctx_query_first_error_code(ctx, &err_code);
    const char *err_msg = nullptr;
    size_t err_msg_len = 0;
    lcb_errctx_query_first_error_message(ctx, &err_msg, &err_msg_len);

    const char *context_id = nullptr;
    size_t context_id_len = 0;
    lcb_errctx_query_client_context_id(ctx, &context_id, &context_id_len);

    const char *statement = nullptr;
    size_t statement_len = 0;
    lcb_errctx_query_statement(ctx, &statement, &statement_len);

    fprintf(stderr,
            "Failed to execute query. lcb: %s, endpoint: %.*s, http_code: %d, "
            "query_code: %d (%.*s), context_id: %.*s, statement: %.*s",
            lcb_strerror_short(lcb_respquery_status(resp)), (int)endpoint_len,
            endpoint, http_code, err_code, (int)err_msg_len, err_msg,
            (int)context_id_len, context_id, (int)statement_len, statement);
}

static void
query_cb(lcb_INSTANCE *instance,
        int cbtype,
        const lcb_RESPQUERY *resp) {
    void *cookie;
    lcb_respquery_cookie(resp, &cookie);
    ResultSet *obj = reinterpret_cast<ResultSet *>(cookie);
    if (lcb_respquery_is_final(resp)) {
        if (obj->scan_consistency == "request_plus" || obj->scan_consistency == "at_plus") {
            if (obj->query_resp_count != 1) {
                fprintf(stderr, "Query count mismatch for stale=false");
            }
        }
        obj->setRescode(lcb_respquery_status(resp) , true);
        if (lcb_respquery_status(resp) != LCB_SUCCESS) {
            dump_http_error(resp);
        }
        return;
    }
    obj->query_resp_count++;
}
}

bool
N1QLQueryExecutor::execute(Command cmd,
                          ResultSet& out,
                          const ResultOptions& options,
                          const Request& req) {

    int iterdelay = req.payload[CBSDKD_MSGFLD_HANDLE_OPTIONS][CBSDKD_MSGFLD_V_QDELAY].asInt();
    std::string consistency = req.payload[CBSDKD_MSGFLD_NQ_SCANCONSISTENCY].asString();
    std::string indexType = req.payload[CBSDKD_MSGFLD_NQ_INDEX_TYPE].asString();
    std::string indexEngine = req.payload[CBSDKD_MSGFLD_NQ_INDEX_ENGINE].asString();
    std::string indexName = req.payload[CBSDKD_MSGFLD_NQ_DEFAULT_INDEX_NAME].asString();
    std::string preparedStr = req.payload[CBSDKD_MSGFLD_NQ_PREPARED].asString();
    bool prepared;
    istringstream(preparedStr) >> std::boolalpha >> prepared;

    std::string scope = "0";
    std::string collection = "0";

    std::vector<std::string> params, paramValues;
    N1QL::split(req.payload[CBSDKD_MSGFLD_NQ_PARAM].asString(), ',', params);
    N1QL::split(req.payload[CBSDKD_MSGFLD_NQ_PARAMVALUES].asString(), ',', paramValues);
    std::string scanConsistency = req.payload[CBSDKD_MSGFLD_NQ_SCANCONSISTENCY].asString();
    params.push_back("handleid");
    paramValues.push_back(std::to_string(handle->hid));

    int ii = 0;
    params.push_back("unique_id");
    paramValues.push_back(std::to_string(ii));

    out.clear();

    handle->externalEnter();

    while(!handle->isCancelled()) {
        out.query_resp_count = 0;
        paramValues.pop_back();
        paramValues.push_back(std::to_string(ii));

        lcb_STATUS err;
        if(!insertDoc(handle->getLcb(), params, paramValues, err)) {
            fprintf(stderr, "Inserting document returned error 0x%x %s\n",
                    err, lcb_strerror_short(err));
        }
        out.scan_consistency = scanConsistency;

        std::string q = std::string("select * from `") + this->handle->options.bucket.c_str() + "`";
        if(handle->options.useCollections){
            q = std::string("select * from `") + collection + "`";
        }

        if (indexType == "secondary")  {
            q += std::string(" where ");
            bool isFirst = true;
            std::vector<std::string>::iterator pit = params.begin();
            std::vector<std::string>::iterator vit = paramValues.begin();

            for(;pit != params.end(); pit++, vit++) {
                if(!isFirst) {
                    q += std::string(" and ");
                } else {
                    isFirst = false;
                }
                q += *pit + std::string("=\"") + *vit + std::string("\" ");
            }
        }

        out.markBegin();
        lcb_CMDQUERY *qcmd;
        lcb_cmdquery_create(&qcmd);
        lcb_cmdquery_callback(qcmd, query_cb);
        lcb_cmdquery_adhoc(qcmd, !prepared);
        if(handle->options.useCollections){
            lcb_cmdquery_scope_name(qcmd, scope.c_str(), scope.size());
        }

        Json::Value bucket_scan_vector;
        bucket_scan_vector[this->handle->options.bucket.c_str()] = tokens;
        if(!N1QL::query(q.c_str(), qcmd, &out, err, LCB_QUERY_CONSISTENCY_NONE)) {
            lcb_cmdquery_destroy(qcmd);
            fprintf(stderr,"Scheduling query returned error 0x%x %s\n",
                    err, lcb_strerror_short(err));
            continue;
        }

        lcb_cmdquery_destroy(qcmd);
        lcb_wait(handle->getLcb(), LCB_WAIT_DEFAULT);
        if (iterdelay) {
            sdkd_millisleep(iterdelay);
        }
        ii++;
    }
    handle->externalLeave();
    return true;
}
}


