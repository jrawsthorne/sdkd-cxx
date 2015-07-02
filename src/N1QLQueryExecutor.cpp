#include "sdkd_internal.h"


namespace CBSdkd {
extern "C" {
static void
insert_cb(lcb_t instance, int type, lcb_RESPBASE *resp) {
    lcb_RESPSTORE *sresp = (lcb_RESPSTORE *)resp;
    N1QLQueryExecutor *obj = reinterpret_cast<N1QLQueryExecutor*>(sresp->cookie);
    if (resp->rc == LCB_SUCCESS) {
        obj->is_isuccess = true;
    } else {
        obj->is_isuccess = false;
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
    doc["id"] = this->doc_index;

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
        return false;
    }
    lcb_sched_leave(instance);
    lcb_wait(instance);
    err = this->insert_err;
    return this->is_isuccess;
}

extern "C" {
static void
query_cb(lcb_t instance,
        int cbtype,
        const lcb_RESPN1QL *resp) {
    ResultSet *obj = reinterpret_cast<ResultSet *>(resp->cookie);
    if (resp->rflags & LCB_RESP_F_FINAL) {
        fprintf(stderr, "insert count %d resp count %d \n",
                obj->query_doc_insert_count,
                obj->query_resp_count);
       if (obj->ryow && obj->query_resp_count != obj->query_doc_insert_count -1) {
            obj->setRescode(Error::SUBSYSf_QUERY || Error::RYOW_MISMATCH, true);
        }
        obj->setRescode(resp->rc , true);
        return;
    }
    obj->query_resp_count++;
}
}
void
N1QLQueryExecutor::split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;

    while(std::getline(ss, item, delim)) {
        if (!item.empty()) {
            elems.push_back(item);
        }
    }
}

bool
N1QLQueryExecutor::execute(Command cmd,
                          ResultSet& out,
                          const ResultOptions& options,
                          const Request& req) {

    int iterdelay = req.payload[CBSDKD_MSGFLD_HANDLE_OPTIONS][CBSDKD_MSGFLD_V_QDELAY].asInt();
    //bool prepared = req.payload[CBSDKD_MSGFLD_NQ_PREPARED].asBool();
    std::string consistency = req.payload[CBSDKD_MSGFLD_NQ_SCANCONSISTENCY].asString();
    std::string indexType = req.payload[CBSDKD_MSGFLD_NQ_INDEX_TYPE].asString();
    std::string indexEngine = req.payload[CBSDKD_MSGFLD_NQ_INDEX_ENGINE].asString();
    std::string indexName = req.payload[CBSDKD_MSGFLD_NQ_DEFAULT_INDEX_NAME].asString();
    std::vector<std::string> params;
    split(req.payload[CBSDKD_MSGFLD_NQ_PARAM].asString(), ',', params);
    std::vector<std::string> paramValues;
    split(req.payload[CBSDKD_MSGFLD_NQ_PARAMVALUES].asString(), ',', paramValues);
    std::string scanConsistency = req.payload[CBSDKD_MSGFLD_NQ_SCANCONSISTENCY].asString();

    out.clear();

    handle->externalEnter();

    while(!handle->isCancelled()) {
        out.query_resp_count = 0;

        lcb_error_t err;
        if(!insertDoc(handle->getLcb(), params, paramValues, err)) {
            fprintf(stderr, "Inserting document returned error 0x%x %s\n",
                    err, lcb_strerror(NULL, err));
            return false;
        }
        out.query_doc_insert_count = this->doc_index;
        this->doc_index++;

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
        qcmd.callback = query_cb;

        if(!N1QL::query(q.c_str(), &qcmd, LCB_N1P_QUERY_STATEMENT, &out, err, consistency)) {
            fprintf(stderr,"Querying returned error 0x%x %s\n",
                    err, lcb_strerror(NULL, err));
        }

        if (iterdelay) {
            sdkd_millisleep(iterdelay);
        }
    }
    handle->externalLeave();
    return true;
}
}


