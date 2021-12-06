#include "sdkd_internal.h"

namespace CBSdkd
{

bool
FTSLoader::populate(const Dataset& ds)
{
    DatasetIterator* iter = ds.getIter();
    int jj = 0;

    for (jj = 0, iter->start(); iter->done() == false; iter->advance(), jj++) {
        std::string k = iter->key();
        std::string v = iter->value();
        pair<string, string> collection = handle->getCollection(k);

        couchbase::document_id id(handle->options.bucket, collection.first, collection.second, k);

        couchbase::operations::upsert_request req{ id, v };
        auto resp = handle->execute(req);
        if (resp.ctx.ec) {
            return false;
        }
    }

    delete iter;
    return true;
}

} // namespace CBSdkd
