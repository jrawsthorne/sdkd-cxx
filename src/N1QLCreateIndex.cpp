#include "sdkd_internal.h"

namespace CBSdkd {
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
                        const Request& req,
                        N1QLConfig *config)
{

    Json::Value ctlopts = req.payload[CBSDKD_MSGFLD_DSREQ_OPTS];
    string indexType = ctlopts[CBSDKD_MSGFLD_NQ_INDEX_TYPE].asString();
    string indexEngine = ctlopts[CBSDKD_MSGFLD_NQ_INDEX_ENGINE].asString();
    string indexName = ctlopts[CBSDKD_MSGFLD_NQ_DEFAULT_INDEX_NAME].asString();

    char qbuf[1024];
    memset(qbuf, '\0', sizeof(qbuf));
    if(sprintf(qbuf, "CREATE PRIMARY INDEX ON `%s` using %s",
                                        this->handle->options.bucket.c_str(),
                                        indexEngine.c_str()) != -1) {
        return false;
    }

    lcb_CMDN1QL qcmd = { 0 };
    qcmd.callback = query_cb;

    if(N1QL::query(qbuf, &qcmd, LCB_N1P_QUERY_STATEMENT, this) &&
            this->success) {
        string params = ctlopts[CBSDKD_MSGFLD_NQ_PARAM].asString();
        string paramValues = ctlopts[CBSDKD_MSGFLD_NQ_PARAMVALUES].asString();

        //construct secondary index on the params
        memset(qbuf, '\0', sizeof(qbuf));
        if (strcmp(params.c_str(), "") == 0  ||
                strcmp(paramValues.c_str(), "") == 0) {
            return true;
        }
        string param = string(strtok((char *)params.c_str(), ","));
        if(sprintf(qbuf,
                    "CREATE INDEX %s ON `%s`(%s) using %s",
                    indexName.c_str(),
                    this->handle->options.bucket.c_str(),
                    param.c_str(),
                    indexEngine.c_str()) != -1) {
            return false;
        }

        memset(&qcmd, '0', sizeof(qcmd));
        qcmd.callback = query_cb;

        if(N1QL::query(qbuf, &qcmd, LCB_N1P_QUERY_PREPARED, this) &&
                this->success){
            if (config) {
                config->set_index_name(indexName);
                config->set_params(params);
                config->set_param_values(paramValues);
            }
            return true;
        }

    }
    return false;
}
}
