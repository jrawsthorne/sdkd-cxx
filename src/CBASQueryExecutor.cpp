#include "sdkd_internal.h"
#include <thread>

namespace CBSdkd {
    extern "C" {
        lcb_MUTATION_TOKEN* mut_cbas;

        static void
        query_cb(lcb_INSTANCE *instance, int cbtype, const lcb_RESPANALYTICS *resp) {
            void *cookie;
            lcb_respanalytics_cookie(resp, &cookie);
            ResultSet *obj = reinterpret_cast<ResultSet *>(cookie);
            if (lcb_respanalytics_is_final(resp)) {
                if (lcb_respanalytics_status(resp) == LCB_SUCCESS && obj->cbas_query_resp_count != 1) {
                    fprintf(stderr, "CBAS response does not match expected number of documents %d\n", obj->cbas_query_resp_count);
                }
                lcb_STATUS status = lcb_respanalytics_status(resp);
                if (status != LCB_SUCCESS) {
                    fprintf(stderr, "CBAS query completed with error = %s\n", lcb_strerror_short(status));
                }
                obj->setRescode(status , true);
                return;
            }

            obj->cbas_query_resp_count++;
        }
    }

    bool CBASQueryExecutor::execute(ResultSet& out,
                            const ResultOptions& options,
                            const Request& req) {
        out.clear();

        handle->externalEnter();
        unsigned int kvCount = req.payload[CBSDKD_MSGFLD_CBAS_COUNT].asInt64();

        // Hardcoded for now until sdkdclient supports more than 1 analytics collection
        std::string scope = "0";
        std::string collection = "0";
        std::string bucket = handle->options.bucket;

        while(!handle->isCancelled()) {
            out.cbas_query_resp_count = 0;

            std::string q = "SELECT * FROM defaultDataSet where `value` = 'SampleValue" + std::to_string(generator) + "'";

            if (handle->options.useCollections) {
                q = "SELECT * FROM `" + collection + "` where `" + collection + "`.`value` = 'SampleValue" + std::to_string(generator) + "'";
            }

            lcb_STATUS err;

            out.markBegin();
            lcb_CMDANALYTICS *qcmd;
            lcb_cmdanalytics_create(&qcmd);
            lcb_cmdanalytics_callback(qcmd, query_cb);

            if (handle->options.useCollections) {
                lcb_cmdanalytics_scope_name(qcmd, scope.c_str(), scope.size());
            }

            lcb_cmdanalytics_statement(qcmd, q.c_str(), strlen(q.c_str()));

            err = lcb_analytics(handle->getLcb(), &out, qcmd);

            if (err != LCB_SUCCESS) {
                lcb_cmdanalytics_destroy(qcmd);
                fprintf(stderr,"Scheduling cbas query returned error 0x%x %s\n",
                        err, lcb_strerror_short(err));
                continue;
            }

            lcb_cmdanalytics_destroy(qcmd);
            lcb_wait(handle->getLcb(), LCB_WAIT_DEFAULT);

            generator = (generator + 1) % kvCount;
        }
        handle->externalLeave();
        return true;
    }
}


