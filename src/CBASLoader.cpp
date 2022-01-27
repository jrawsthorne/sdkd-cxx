#include "sdkd_internal.h"

namespace CBSdkd
{
bool
CBASLoader::populate(const Dataset& ds)
{
    DatasetIterator* iter = ds.getIter();
    int batch = 100;
    int jj = 0;

    for (jj = 0, iter->start(); iter->done() == false; iter->advance(), jj++) {
        std::string k = iter->key();
        std::string v = iter->value();
        pair<string, string> collection = handle->getCollection(k);

        couchbase::document_id id(handle->options.bucket, collection.first, collection.second, k);

        couchbase::operations::upsert_request req{ id, v };
        handle->pending_futures.emplace_back(handle->execute_async_ec(req));

        if (jj % batch == 0) {
            bool ok = true;
            handle->drainPendingFutures([&ok](std::error_code ec) { ok = ok && !ec; });
            if (!ok) {
                return false;
            }
        }
    }

    handle->drainPendingFutures([](std::error_code ec) {});

    delete iter;
    return true;
}

} // namespace CBSdkd
