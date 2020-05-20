//
// Created by Will Broadbelt on 18/05/2020.
//

#include "sdkd_internal.h"

//Helper function to coerce http response
lcb_STATUS lcb_http_status(const uint16_t status, const char *const body) {
    lcb_STATUS rc;
    switch (status) {
        case 200:
            rc = LCB_SUCCESS;
            break;
        case 400:
            if (strcmp(body, "\"Bucket with given name already exists\"") == 0) {
                rc = LCB_ERR_BUCKET_ALREADY_EXISTS;
            } else if (strcmp(body, "\"Scope with this name already exists\"") == 0) {
                rc = LCB_ERR_SCOPE_EXISTS;
            } else if (strcmp(body, "\"Collection with this name already exists\"") == 0) {
                rc = LCB_ERR_COLLECTION_ALREADY_EXISTS;
            } else {
                rc = LCB_ERR_GENERIC;
            }
            break;
        case 404:
            if (strcmp(body, "\"User not found\"") == 0) {
                rc = LCB_ERR_USER_NOT_FOUND;
            } else if (strcmp(body, "\"Group not found\"") == 0) {
                rc = LCB_ERR_GROUP_NOT_FOUND;
            } else {
                rc = LCB_ERR_GENERIC;
            }
            break;
        default:
            rc = LCB_ERR_GENERIC;
            break;
    }
    return rc;
}

static void print_err(lcb_INSTANCE *instance, const char *msg, lcb_STATUS err) {
    fprintf(stderr, "%s. Received code 0x%X (%s)\n", msg, err, lcb_strerror_short(err));
}

void http_callback(lcb_INSTANCE *instance, int cbtype, const lcb_RESPHTTP *resp) {
    uint16_t status;
    lcb_STATUS *cookie;
    lcb_resphttp_cookie(resp, reinterpret_cast<void **>(&cookie));
    lcb_resphttp_http_status(resp, &status);
    const char *const *headers;
    lcb_resphttp_headers(resp, &headers);
    const char *body;
    size_t nbody;
    lcb_resphttp_body(resp, &body, &nbody);

    *cookie = lcb_http_status(status, body);

    lcb_STATUS rc = lcb_resphttp_status(resp);
    if (rc != LCB_SUCCESS) {
        print_err(instance, "Failed to execute HTTP request", rc);
    }
}

lcb_STATUS Collections::create_scope(lcb_INSTANCE *instance, const std::string &scope) {
    lcb_install_callback(instance, LCB_CALLBACK_HTTP, (lcb_RESPCALLBACK) http_callback);

    lcb_CMDHTTP *cmd;
    lcb_STATUS err;
    std::string path = "/pools/default/buckets/default/collections";
    std::string body = "name=" + scope;
    std::string content_type = "application/x-www-form-urlencoded";

    lcb_cmdhttp_create(&cmd, LCB_HTTP_TYPE_MANAGEMENT);
    lcb_cmdhttp_method(cmd, LCB_HTTP_METHOD_POST);
    lcb_cmdhttp_content_type(cmd, content_type.c_str(), content_type.size());
    lcb_cmdhttp_path(cmd, path.c_str(), path.size());
    lcb_cmdhttp_body(cmd, body.c_str(), body.size());

    lcb_STATUS cookie;
    err = lcb_http(instance, &cookie, cmd);
    lcb_cmdhttp_destroy(cmd);
    if (err != LCB_SUCCESS) {
        print_err(instance, "Failed command to create scope", err);
        return err;
    }
    lcb_wait(instance, LCB_WAIT_DEFAULT);

    return cookie;
}

lcb_STATUS
Collections::create_collection(lcb_INSTANCE *instance, const std::string &scope, const std::string &collection) {
    lcb_install_callback(instance, LCB_CALLBACK_HTTP, (lcb_RESPCALLBACK) http_callback);

    lcb_CMDHTTP *cmd;
    lcb_STATUS err;
    std::string path = "/pools/default/buckets/default/collections/" + scope + "/";
    std::string body = "name=" + collection;
    std::string content_type = "application/x-www-form-urlencoded";

    lcb_cmdhttp_create(&cmd, LCB_HTTP_TYPE_MANAGEMENT);
    lcb_cmdhttp_method(cmd, LCB_HTTP_METHOD_POST);
    lcb_cmdhttp_content_type(cmd, content_type.c_str(), content_type.size());
    lcb_cmdhttp_path(cmd, path.c_str(), path.size());
    lcb_cmdhttp_body(cmd, body.c_str(), body.size());

    lcb_STATUS cookie;
    err = lcb_http(instance, &cookie, cmd);
    lcb_cmdhttp_destroy(cmd);

    if (err != LCB_SUCCESS) {
        print_err(instance, "Failed to create collection", err);
        return err;
    }
    lcb_wait(instance, LCB_WAIT_DEFAULT);

    return cookie;
}

lcb_STATUS Collections::drop_scope(lcb_INSTANCE *instance, const std::string &scope) {
    lcb_CMDHTTP *cmd;
    lcb_STATUS err;
    std::string path = "/pools/default/buckets/default/collections/" + scope;

    lcb_cmdhttp_create(&cmd, LCB_HTTP_TYPE_MANAGEMENT);
    lcb_cmdhttp_method(cmd, LCB_HTTP_METHOD_DELETE);
    lcb_cmdhttp_path(cmd, path.c_str(), path.size());

    err = lcb_http(instance, NULL, cmd);
    lcb_cmdhttp_destroy(cmd);
    if (err != LCB_SUCCESS) {
        print_err(instance, "Failed to delete scope.", err);
    }
    return lcb_wait(instance, LCB_WAIT_DEFAULT);
}


lcb_STATUS
Collections::drop_collection(lcb_INSTANCE *instance, const std::string &scope, const std::string &collection) {
    lcb_CMDHTTP *cmd;
    lcb_STATUS err;
    std::string path = "/pools/default/buckets/default/collections/" + scope + "/" + collection;

    lcb_cmdhttp_create(&cmd, LCB_HTTP_TYPE_MANAGEMENT);
    lcb_cmdhttp_method(cmd, LCB_HTTP_METHOD_DELETE);
    lcb_cmdhttp_path(cmd, path.c_str(), path.size());

    err = lcb_http(instance, NULL, cmd);
    lcb_cmdhttp_destroy(cmd);
    if (err != LCB_SUCCESS) {
        print_err(instance, "Failed to delete collection.", err);
    }
    return lcb_wait(instance, LCB_WAIT_DEFAULT);
}

lcb_STATUS Collections::list_collections(lcb_INSTANCE *instance, const std::string &bucket) {
    lcb_CMDHTTP *cmd;
    lcb_STATUS err;
    std::string path = "/pools/default/buckets/" + bucket + "/collections";

    lcb_cmdhttp_create(&cmd, LCB_HTTP_TYPE_MANAGEMENT);
    lcb_cmdhttp_method(cmd, LCB_HTTP_METHOD_GET);
    lcb_cmdhttp_path(cmd, path.c_str(), path.size());

    err = lcb_http(instance, NULL, cmd);
    lcb_cmdhttp_destroy(cmd);
    if (err != LCB_SUCCESS) {
        print_err(instance, "Failed to delete collection.", err);
    }
    return lcb_wait(instance, LCB_WAIT_DEFAULT);
}

bool Collections::generateCollections(lcb_INSTANCE *instance, int scopes, int collections) {
    if (!collectionsGenerated.load()) {
        collectionsGenerated = true;
        printf("Creating %d scopes, and %d collections.\n", scopes, collections*scopes);
        lcb_STATUS rc;
        for (int i = 0; i < scopes; ++i) {
            std::string scope = std::to_string(i);
            int start = i * collections;
            rc = create_scope(instance, scope);
            if (rc != LCB_SUCCESS) {
                fprintf(stderr, "Failed creating scope %s. Got error %s\n", scope.c_str(), lcb_strerror_short(rc));
                return false;
            }
            for (int j = i * collections; j < start + collections; ++j) {
                rc = create_collection(instance, scope, std::to_string(j));
                if (rc != LCB_SUCCESS) {
                    fprintf(stderr, "Failed creating collection %d . Got error: %s\n", j, lcb_strerror_short(rc));
                    return false;
                }
            }
        }
        printf("Successfully created the requested scopes and collections.\n");
        return true;
    }
    return false; //Collections already generated
}

