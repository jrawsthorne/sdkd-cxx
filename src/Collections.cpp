//
// Created by Will Broadbelt on 18/05/2020.
//

#include "sdkd_internal.h"

namespace CBSdkd
{

std::error_code
Collections::create_scope(Handle* handle, const std::string& scope)
{
    couchbase::core::operations::management::scope_create_request req{ handle->options.bucket, scope };
    auto resp = handle->execute(req);
    return resp.ctx.ec;
}

std::error_code
Collections::create_collection(Handle* handle, const std::string& scope, const std::string& collection)
{
    couchbase::core::operations::management::collection_create_request req{ handle->options.bucket, scope, collection };
    auto resp = handle->execute(req);
    return resp.ctx.ec;
}

bool
Collections::generateCollections(Handle* handle, int scopes, int collections)
{
    if (!collectionsGenerated.load()) {
        collectionsGenerated = true;
        printf("Creating %d scopes, and %d collections.\n", scopes, collections * scopes);
        std::error_code ec;
        for (int i = 0; i < scopes; ++i) {
            std::string scope = std::to_string(i);
            int start = i * collections;
            ec = create_scope(handle, scope);
            if (ec) {
                fprintf(stderr, "Failed creating scope %s. Got error %s\n", scope.c_str(), ec.message().c_str());
                return false;
            }
            for (int j = i * collections; j < start + collections; ++j) {
                ec = create_collection(handle, scope, std::to_string(j));
                if (ec) {
                    fprintf(stderr, "Failed creating collection %d . Got error: %s\n", j, ec.message().c_str());
                    return false;
                }
            }
        }
        printf("Successfully created the requested scopes and collections.\n");
        return true;
    }
    return false; // Collections already generated
}

} // namespace CBSdkd