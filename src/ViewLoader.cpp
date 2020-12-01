#include "sdkd_internal.h"

namespace CBSdkd {
using namespace std;

#define VIEWLOAD_BATCH_COUNT 1000

extern "C"
{
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
                "Failed to retry store operation %p in ViewLoader: key=\"%.*s\" "
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
      dump_key_value_error("Failed to load document in ViewLoader", ctx);
      delete op;
    }
  }
}
}

ViewLoader::ViewLoader(Handle *handle)
{
    this->handle = handle;
}

void ViewLoader::flushValues(ResultSet& rs)
{
    lcb_install_callback(handle->getLcb(), LCB_CALLBACK_STORE,
                       reinterpret_cast<lcb_RESPCALLBACK>(cb_store));

    for (auto & value : values) {
        auto *op = new store_op(value.key, value.value, handle->getCollection(value.key));
        lcb_STATUS err = lcb_store(handle->getLcb(), op, op->cmd_);
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
