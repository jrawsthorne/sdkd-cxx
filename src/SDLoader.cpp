#include "sdkd_internal.h"

namespace CBSdkd {
extern "C" {

static void cb_store(lcb_INSTANCE *instance, int, const lcb_RESPSTORE *resp) {
  lcb_STATUS rc = lcb_respstore_status(resp);
  store_op *op = nullptr;
  lcb_respstore_cookie(resp, reinterpret_cast<void **>(&op));
  if (rc == LCB_SUCCESS) {
    delete op;
  } else {
    const lcb_KEY_VALUE_ERROR_CONTEXT *ctx;
    lcb_respstore_error_context(resp, &ctx);

    if (rc == LCB_ERR_TIMEOUT || rc == LCB_ERR_BUCKET_NOT_FOUND) {
      lcb_STATUS err = lcb_store(instance, op, op->cmd_);
      if (err != LCB_SUCCESS) {
        fprintf(stderr,
                "Failed to retry store operation %p in SDLoader: key=\"%.*s\" "
                "rc=%s, err=%s\n",
                (void *)op, (int)op->key_.size(), op->key_.c_str(),
                lcb_strerror_short(rc), lcb_strerror_short(err));
        delete op;
        return;
      }
      fprintf(stderr,
              "Retrying store operation %p in SDLoader: key=\"%.*s\", rc=%s\n",
              (void *)op, (int)op->key_.size(), op->key_.c_str(),
              lcb_strerror_short(rc));
    } else {
      dump_key_value_error("Failed to load document in SDLoader", ctx);
      delete op;
    }
  }
}
}

bool
SDLoader::populate(const Dataset& ds, ResultSet& out, const ResultOptions& opts) {
    out.options = opts;
    out.clear();

    DatasetIterator *iter = ds.getIter();
    int batch = 100;
    int ii = 0, jj = 0;
    lcb_install_callback(handle->getLcb(), LCB_CALLBACK_STORE,
                         reinterpret_cast<lcb_RESPCALLBACK>(cb_store));

    lcb_sched_enter(handle->getLcb());
    for (ii=0, jj=0, iter->start(); iter->done() == false; iter->advance(), ii++, jj++) {
        auto *op = new store_op(iter->key(), iter->value(), handle->getCollection(iter->key()));
        lcb_STATUS err = lcb_store(handle->getLcb(), op, op->cmd_);
        if (err != LCB_SUCCESS) {
            delete op;
            fprintf(stderr, "Failed to schedule store operation in SDLoader: err=%s\n",
                    lcb_strerror_short(err));
            lcb_sched_fail(handle->getLcb());
            delete iter;
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
