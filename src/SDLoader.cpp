#include "sdkd_internal.h"

namespace CBSdkd {
extern "C" {
static void cb_store(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp) {
}
}

bool
SDLoader::populate(const Dataset& ds, ResultSet& out, const ResultOptions& opts) {
    out.options = opts;
    out.clear();

    DatasetIterator *iter = ds.getIter();
    int batch = 100;
    int ii = 0, jj = 0;
    lcb_install_callback(handle->getLcb(), LCB_CALLBACK_STORE, cb_store);

    lcb_sched_enter(handle->getLcb());
    for (ii=0, jj=0, iter->start(); iter->done() == false; iter->advance(), ii++, jj++) {
        std::string k = iter->key();
        std::string v = iter->value();
        pair<string, string> collection = handle->getCollection(k);

        lcb_CMDSTORE *cmd;
        lcb_cmdstore_create(&cmd, LCB_STORE_UPSERT);
        if(collection.first.length() != 0) {
            lcb_cmdstore_collection(cmd, collection.first.c_str(), collection.first.size(), collection.second.c_str(), collection.second.size());
        }
        lcb_cmdstore_key(cmd, k.data(), k.size());
        lcb_cmdstore_value(cmd, v.data(), v.size());
        lcb_STATUS err = lcb_store(handle->getLcb(), NULL, cmd);
        lcb_cmdstore_destroy(cmd);
        if (err != LCB_SUCCESS) {
            return false;
        }
        if (jj % batch == 0) {
            lcb_sched_leave(handle->getLcb());
            lcb_wait(handle->getLcb(), LCB_WAIT_DEFAULT);
            lcb_sched_enter(handle->getLcb());
        }
    }
    lcb_sched_leave(handle->getLcb());
    lcb_wait(handle->getLcb(), LCB_WAIT_DEFAULT);

    delete iter;
    return true;
}

}
