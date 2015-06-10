#include "sdkd_internal.h"


namespace CBSdkd {



bool
N1QLQueryExecutor::execute(Command cmd,
                          ResultSet& out,
                          const ResultOptions& options,
                          const Request& req,
                          N1QLConfig *config) {

    Json::Value ctlopts = req.payload[CBSDKD_MSGFLD_DSREQ_OPTS];
    int iterdelay = ctlopts[CBSDKD_MSGFLD_V_QDELAY].asInt();
    bool prepared = ctlopts[CBSDKD_MSGFLD_NQ_PREPARED].asBoolean();
    string scanConsistency = ctlopts[CBSDKD_MSGFLD_NQ_SCANCONSISTENCY].asString();
    string params = config.get_params();
    string paramValues = config.get_param_values();
    string indexName = config.get_index_name();

    if(prepared) {
        //execute prepared statement
    }

    handle->externalEnter();

    while(!handle->isCancelled()) {
        rs->markBegin();
        insertDoc(params, paramValues);
        //constructQuery(

        if (iterdelay) {
            sdkd_millisleep(iterdelay);
        }
    }

    handle->externalLeave();

    return true;
}
}


