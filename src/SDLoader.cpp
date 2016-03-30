#include "sdkd_internal.h"

namespace CBSdkd {
extern "C" {
static void cb_store(lcb_t instance, int, const lcb_RESPBASE *resp) {
}
}

bool
SDLoader::populate(const Dataset& ds, ResultSet& out, const ResultOptions& opts) {
    out.options = opts;
    out.clear();

    DatasetIterator *iter = ds.getIter();
    int batch = 100;
    int ii = 0, jj = 0;
    lcb_install_callback3(handle->getLcb(), LCB_CALLBACK_STORE, cb_store);

    lcb_sched_enter(handle->getLcb());
    for (ii=0, jj=0, iter->start(); iter->done() == false; iter->advance(), ii++, jj++) {
        std::string k = iter->key();
        std::string v = iter->value();

        lcb_CMDSTORE cmd = { 0 };
        cmd.operation = LCB_SET;
        cmd.value.vtype = LCB_KV_COPY;

        LCB_CMD_SET_KEY(&cmd, k.data(), k.size());
        cmd.value.u_buf.contig.bytes = v.data();
        cmd.value.u_buf.contig.nbytes = v.size();
        lcb_error_t err = lcb_store3(handle->getLcb(), NULL, &cmd);
        if (err != LCB_SUCCESS) {
            return false;
        }
        if (jj % batch == 0) {
            lcb_sched_leave(handle->getLcb());
            lcb_wait(handle->getLcb());
            lcb_sched_enter(handle->getLcb());
        }
    }
    lcb_sched_leave(handle->getLcb());
    lcb_wait(handle->getLcb());

    delete iter;
    return true;
}

}
