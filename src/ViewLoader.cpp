#include "sdkd_internal.h"

namespace CBSdkd {
using namespace std;

#define VIEWLOAD_BATCH_COUNT 1000

extern "C"
{
static void
cb_store(lcb_t instance, const void *cookie,
         lcb_storage_t oper, lcb_error_t err,
         const lcb_store_resp_t *resp)
{
    ResultSet *rs = reinterpret_cast<ResultSet*>((void*)cookie);
    rs->setRescode(err, resp->v.v0.key, resp->v.v0.nkey);
}
}

ViewLoader::ViewLoader(Handle *handle)
{
    this->handle = handle;
}

void ViewLoader::flushValues(ResultSet& rs)
{
    vector<lcb_store_cmd_t> scmds;
    vector<const lcb_store_cmd_t*> scmds_p;

    for (kvp_list::iterator iter = values.begin();
            iter != values.end();
            iter++) {

        lcb_store_cmd_t cmd = lcb_store_cmd_st();
        cmd.v.v0.key = iter->key.c_str();
        cmd.v.v0.nkey = iter->key.size();

        cmd.v.v0.bytes = (const void*)iter->value.c_str();
        cmd.v.v0.nbytes = iter->value.size();
        cmd.v.v0.operation = LCB_SET;
        scmds.push_back(cmd);
    }

    for (unsigned int ii = 0; ii < scmds.size(); ii++) {
        scmds_p.push_back(&scmds[ii]);
    }

    lcb_store_callback old_cb = lcb_set_store_callback(handle->getLcb(),
                                                       cb_store);

    lcb_error_t err = lcb_store(handle->getLcb(), &rs,
                                scmds_p.size(),
                                &scmds_p[0]);

    if (err != LCB_SUCCESS) {
        rs.setRescode(err);

    } else {
        lcb_wait(handle->getLcb());
    }

    lcb_set_store_callback(handle->getLcb(), old_cb);
}

bool ViewLoader::populateViewData(Command cmd,
                                  const Dataset& ds,
                                  ResultSet& out,
                                  const ResultOptions& options,
                                  const Request& req)
{
    Json::Value schema = req.payload[CBSDKD_MSGFLD_V_SCHEMA];
    Json::FastWriter jwriter;

    out.clear();
    out.options = options;

    int ii = 0;
    DatasetIterator *iter = ds.getIter();

    handle->externalEnter();

    for (ii = 0, iter->start(); iter->done() == false; iter->advance(), ii++) {

        schema[CBSDKD_MSGFLD_V_KIDENT] = iter->key();
        schema[CBSDKD_MSGFLD_V_KSEQ] = ii;
        _kvp kvp;
        kvp.key = iter->key();
        kvp.value = jwriter.write(schema);
        values.push_back(kvp);

        if (values.size() >= VIEWLOAD_BATCH_COUNT) {
            flushValues(out);
            values.clear();
        }
    }

    if (!values.empty()) {
        flushValues(out);
    }

    handle->externalLeave();

    return true;
}

}
