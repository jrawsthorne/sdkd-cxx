//
// Created by Will Broadbelt on 18/05/2020.
//

#ifndef SDKD_CPP_COLLECTIONS_H
#define SDKD_CPP_COLLECTIONS_H

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#include <atomic>
#include <couchbase/operations/management/collections.hxx>

namespace CBSdkd
{
class Collections
{
  public:
    static Collections& getInstance()
    {
        static Collections instance;
        return instance;
    }

    std::error_code create_scope(Handle* handle, const std::string& scope);

    std::error_code create_collection(Handle* handle, const std::string& scope, const std::string& collection);

    bool generateCollections(Handle* handle, int scopes, int collections);

  private:
    Collections()
    {
        collectionsGenerated = false;
    }

    std::atomic<bool> collectionsGenerated;
};
} // namespace CBSdkd

#endif // SDKD_CPP_COLLECTIONS_H
