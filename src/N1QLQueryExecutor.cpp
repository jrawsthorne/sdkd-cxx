#include "sdkd_internal.h"


namespace CBSdkd {
extern "C" {
static void
insert_cb(lcb_t instance, int type, lcb_RESPBASE *resp) {
    lcb_RESPSTORE *sresp = (lcb_RESPSTORE *)resp;
    N1QLQueryExecutor *obj = reinterpret_cast<N1QLQueryExecutor*>(sresp->cookie);
    if (resp->rc == LCB_SUCCESS) {
        obj->is_isuccess = true;
        lcb_MUTATION_TOKEN ss = *lcb_resp_get_mutation_token(type, resp);
        Json::Value vbucket;
        vbucket["guard"] = std::to_string(LCB_MUTATION_TOKEN_ID(&ss));
        vbucket["value"]  = (unsigned int)LCB_MUTATION_TOKEN_SEQ(&ss);
        obj->tokens[std::to_string(LCB_MUTATION_TOKEN_VB(&ss))] = vbucket;
        std::string val = Json::FastWriter().write(obj->tokens);
    }
    obj->insert_err = resp->rc;
}
}

bool
N1QLQueryExecutor::insertDoc(lcb_t instance,
        std::vector<std::string> &params,
        std::vector<std::string> &paramValues,
        lcb_error_t& err) {

    std::vector<std::string>::iterator pit = params.begin();
    std::vector<std::string>::iterator vit = paramValues.begin();

    Json::Value doc;
    doc["id"] = std::to_string(handle->hid);

    for(;pit<params.end(); pit++, vit++) {
        doc[*pit] = *vit;
    }

    std::string val = Json::FastWriter().write(doc);
    std::string key = doc["id"].asString();

    lcb_install_callback3(instance, LCB_CALLBACK_STORE, (lcb_RESPCALLBACK)insert_cb);
    lcb_CMDSTORE scmd = { 0 };

    LCB_CMD_SET_KEY(&scmd, key.c_str(), key.size());
    LCB_CMD_SET_VALUE(&scmd, val.c_str(), val.size());
    scmd.operation = LCB_SET;

    lcb_sched_enter(instance);
    err = lcb_store3(instance, (void *)this, &scmd);
    if (err != LCB_SUCCESS) {
        this->insert_err = err;
        return false;
    }
    lcb_sched_leave(instance);
    lcb_wait(instance);
    err = this->insert_err;
    if (err != LCB_SUCCESS) {
        return false;
    }
    return true;
}

extern "C" {
static void
query_cb(lcb_t instance,
        int cbtype,
        const lcb_RESPN1QL *resp) {
    ResultSet *obj = reinterpret_cast<ResultSet *>(resp->cookie);
    if (resp->rflags & LCB_RESP_F_FINAL) {
        if (obj->scan_consistency == "request_plus" || obj->scan_consistency == "at_plus")          {
            if (obj->query_resp_count != 1) {
                obj->setRescode(Error::SUBSYSf_QUERY || Error::RYOW_MISMATCH, true);
            }
        }
        obj->setRescode(resp->rc , true);
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
    bool isPrepared = req.payload[CBSDKD_MSGFLD_NQ_PREPARED].asBool();

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

        lcb_error_t err;
        if(!insertDoc(handle->getLcb(), params, paramValues, err)) {
            fprintf(stderr, "Inserting document returned error 0x%x %s\n",
                    err, lcb_strerror(NULL, err));
        }
        out.scan_consistency = scanConsistency;

        std::string q = std::string("select * from `") + this->handle->options.bucket.c_str() + "`";

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
        lcb_CMDN1QL qcmd = { 0 };
        if (isPrepared) {
            qcmd.cmdflags |= LCB_CMDN1QL_F_PREPCACHE;
        }

        qcmd.callback = query_cb;
        Json::Value scan_vector = tokens;
        if(!N1QL::query(q.c_str(), &qcmd, LCB_N1P_QUERY_STATEMENT, &out, err, consistency, scan_vector)) {
            fprintf(stderr,"Scheduling query returned error 0x%x %s\n",
                    err, lcb_strerror(NULL, err));
            continue;
        }

        lcb_wait(handle->getLcb());
        if (iterdelay) {
            sdkd_millisleep(iterdelay);
        }
        ii++;
    }
    handle->externalLeave();
    return true;
}
}


