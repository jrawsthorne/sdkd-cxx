#include "sdkd_internal.h"

namespace CBSdkd {

extern "C" {
static void
query_cb(lcb_t, int, const lcb_RESPN1QL *resp) {
    N1QLCreateIndex *obj = static_cast<N1QLCreateIndex *>(resp->cookie);
    if (resp->rc == LCB_SUCCESS) {
        obj->is_qsuccess = true;
        obj->rc = resp->rc;
    } else {
        obj->is_qsuccess = false;
        obj->rc = resp->rc;
        exit(1);
    }
}
}



bool
N1QLCreateIndex::execute(Command cmd,
                        const Request& req) {

    std::string indexType = req.payload[CBSDKD_MSGFLD_NQ_INDEX_TYPE].asString();
    std::string indexEngine = req.payload[CBSDKD_MSGFLD_NQ_INDEX_ENGINE].asString();
    std::string indexName = req.payload[CBSDKD_MSGFLD_NQ_DEFAULT_INDEX_NAME].asString();
    std::string params = req.payload[CBSDKD_MSGFLD_NQ_PARAM].asString();


    char qbuf[1024];
    memset(qbuf, '\0', sizeof(qbuf));
    if(sprintf(qbuf, "CREATE PRIMARY INDEX ON `%s` using %s",
                                        this->handle->options.bucket.c_str(),
                                        indexEngine.c_str()) == -1) {
        return false;
    }

    lcb_CMDN1QL qcmd = { 0 };
    qcmd.callback = query_cb;
    lcb_error_t scherr;
    std::string consistency = "not_bounded";
    Json::Value scan_vector;

    if(N1QL::query(qbuf, &qcmd, LCB_N1P_QUERY_STATEMENT, (void *)this, scherr, consistency, scan_vector)) {
        lcb_wait(handle->getLcb());
        if (!this->is_qsuccess) {
            fprintf(stderr, "Create primary index failed 0x%x %s \n",
                    this->rc, lcb_strerror(NULL, this->rc));
            return false;
        }
    } else {
        if (scherr !=  LCB_SUCCESS) {
            fprintf(stderr, "Scheduling primary index command failed 0x%x %s \n",
                    scherr, lcb_strerror(NULL, scherr));
        }
        return false;
    }

    if(indexType == "secondary") {
        //construct secondary index on the params
        memset(qbuf, '\0', sizeof(qbuf));

        sprintf(qbuf,
                "CREATE INDEX %s ON `%s`(%s) using %s",
                indexName.c_str(),
                this->handle->options.bucket.c_str(),
                params.c_str(),
                indexEngine.c_str());

        memset(&qcmd, '\0', sizeof(qcmd));
        qcmd.callback = query_cb;


        if(N1QL::query(qbuf, &qcmd, LCB_N1P_QUERY_STATEMENT, (void *)this, scherr, consistency, scan_vector)){
            lcb_wait(handle->getLcb());
            if (!this->is_qsuccess) {
                fprintf(stderr, "Create secondary index command failed 0x%x %s \n",
                        this->rc, lcb_strerror(NULL, this->rc));
                return false;
            }
        } else {
            if (scherr !=  LCB_SUCCESS) {
                fprintf(stderr, "Scheduling secondary index command failed 0x%x %s \n",
                        scherr, lcb_strerror(NULL, scherr));
            }
            return false;
        }
    }
    return true;
}
}
