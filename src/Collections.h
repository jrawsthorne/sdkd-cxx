//
// Created by Will Broadbelt on 18/05/2020.
//

#ifndef SDKD_CPP_COLLECTIONS_H
#define SDKD_CPP_COLLECTIONS_H

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#include <atomic>

class Collections {
public:
    static Collections &getInstance() {
        static Collections instance;
        return instance;
    }

    static lcb_STATUS create_scope(lcb_INSTANCE *instance, const std::string &scope);

    lcb_STATUS create_collection(lcb_INSTANCE *instance, const std::string &scope, const std::string &collection);

    lcb_STATUS drop_scope(lcb_INSTANCE *instance, const std::string &scope);

    lcb_STATUS drop_collection(lcb_INSTANCE *instance, const std::string &scope, const std::string &collection);

    lcb_STATUS list_collections(lcb_INSTANCE *instance, const std::string &bucket);

    bool generateCollections(lcb_INSTANCE *instance, int scopes, int collections);


private:
    Collections() {
        collectionsGenerated = false;
    }

    std::atomic<bool> collectionsGenerated;
};


#endif //SDKD_CPP_COLLECTIONS_H
