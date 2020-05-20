#include "sdkd_internal.h"

namespace CBSdkd {
using namespace std;

#define VIEWLOAD_BATCH_COUNT 1000

extern "C"
{
static void cb_store(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp) {
}
}

ViewLoader::ViewLoader(Handle *handle)
{
    this->handle = handle;
}

void ViewLoader::flushValues(ResultSet& rs)
{
    lcb_install_callback(handle->getLcb(), LCB_CALLBACK_STORE, cb_store);

    for (kvp_list::iterator iter = values.begin();
            iter != values.end();
            iter++) {

        pair<string, string> collection = handle->getCollection(iter->key);
        lcb_CMDSTORE *cmd;
        lcb_cmdstore_create(&cmd, LCB_STORE_UPSERT);
        lcb_cmdstore_collection(cmd, collection.first.c_str(), collection.first.size(), collection.second.c_str(), collection.second.size());
        lcb_cmdstore_key(cmd, iter->key.c_str(), iter->key.size());
        lcb_cmdstore_value(cmd, iter->value.c_str(), iter->value.size());

        lcb_STATUS err = lcb_store(handle->getLcb(), NULL, cmd);
        lcb_cmdstore_destroy(cmd);
        if (err != LCB_SUCCESS) {
            rs.setRescode(err);
        }
    }
    lcb_wait(handle->getLcb(), LCB_WAIT_DEFAULT);
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
