/*
 * Handle.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include <regex>
#include "sdkd_internal.h"

namespace CBSdkd {


extern "C" {

void Handle::VersionInfoJson(Json::Value &res) {
    Json::Value caps;
    Json::Value config;
    Json::Value rtComponents;
    Json::Value hdrComponents;
    char vbuf[1000] = { 0 };

    const DaemonOptions& dOpts =
            Daemon::MainDaemon
                ? Daemon::MainDaemon->getOptions()
                : DaemonOptions();

    lcb_uint32_t vout = 0;
    lcb_get_version(&vout);
    sprintf(vbuf, "0x%X", vout);
    rtComponents["SDKVersion"] = vbuf;

    sprintf(vbuf, "0x%x", LCB_VERSION);
    hdrComponents["SDKVersion"] = vbuf;

// Thanks mauke
#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)
    hdrComponents["CHANGESET"] = LCB_VERSION_CHANGESET;
    fprintf(stderr, " SDK version changeset %s\n", hdrComponents["CHANGESET"].asString().c_str());
#undef STRINGIFY
#undef STRINGIFY_

    config["IO_PLUGIN"] = dOpts.ioPluginName ? dOpts.ioPluginName : "";
    config["CONNCACHE"] = dOpts.conncachePath ? dOpts.conncachePath : "";
    config["RECONNECT"] = dOpts.noPersist;

    caps["CANCEL"] = true;
    caps["DS_SHARED"] = true;
    caps["CONTINUOUS"] = true;
    caps["PREAMBLE"] = false;
    caps["VIEWS"] = true;

    res["CAPS"] = caps;
    res["RUNTIME"] = rtComponents;
    res["HEADERS"] = hdrComponents;
    res["CONFIG"] = config;
    res["TIME"] = (Json::UInt64)time(NULL);
    res["SDK"] = "libcouchbase" ;
    res["CHANGESET"] = hdrComponents["CHANGESET"];
}

static void cb_config(lcb_INSTANCE *instance, lcb_STATUS err)
{
    (void)instance;
    if (err != LCB_SUCCESS) {
        log_noctx_error("Got error 0x%x", err);
    }
}


static void cb_remove(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp)
{
    const lcb_RESPREMOVE *rb = (const lcb_RESPREMOVE *)resp;
    void *cookie;
    const char* key;
    size_t nkey;
    lcb_respremove_key(rb, &key, &nkey);
    lcb_respremove_cookie(rb, &cookie);
    reinterpret_cast<ResultSet*>(cookie)->setRescode(lcb_respremove_status(rb),
            key, nkey);
}

static void cb_touch(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp)
{
    const lcb_RESPTOUCH *rb = (const lcb_RESPTOUCH *)resp;
    void *cookie;
    const char* key;
    size_t nkey;
    lcb_resptouch_key(rb, &key, &nkey);
    lcb_resptouch_cookie(rb, &cookie);
    reinterpret_cast<ResultSet*>(cookie)->setRescode(lcb_resptouch_status(rb),
            key, nkey);
}

static void cb_storage(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp)
{
    const lcb_RESPSTORE *rb = (const lcb_RESPSTORE *)resp;
    void *cookie;
    const char* key;
    size_t nkey;
    lcb_respstore_key(rb, &key, &nkey);
    lcb_respstore_cookie(rb, &cookie);
    reinterpret_cast<ResultSet*>(cookie)->setRescode(lcb_respstore_status(rb),
            key, nkey);
}

static void cb_storedur(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp)
{
    const lcb_RESPSTORE *rb = (const lcb_RESPSTORE *)resp;
    void *cookie;
    const char* key;
    size_t nkey;
    lcb_respstore_key(rb, &key, &nkey);
    lcb_respstore_cookie(rb, &cookie);
    reinterpret_cast<ResultSet*>(cookie)->setRescode(lcb_respstore_status(rb),
            key, nkey);
}

static void cb_get(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp)
{
    const lcb_RESPGET *rb = (const lcb_RESPGET *)resp;
    void *cookie;
    const char* key;
    size_t nkey;
    const char* value;
    size_t nvalue;
    lcb_respget_key(rb, &key, &nkey);
    lcb_respget_value(rb, &value, &nvalue);
    lcb_respget_cookie(rb, &cookie);
    reinterpret_cast<ResultSet*>(cookie)->setRescode(lcb_respget_status(rb),
            key, nkey, true, value, nvalue);
}

static void cb_endure(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp)
{
    lcb_RESPSTORE* dresp = (lcb_RESPSTORE *)resp;
    void *cookie;
    const char* key;
    size_t nkey;
    int persisted_master;
    lcb_respstore_key(dresp, &key, &nkey);
    lcb_respstore_cookie(dresp, &cookie);
    lcb_respstore_observe_master_persisted(dresp, &persisted_master);
    if (persisted_master == 0) {
        reinterpret_cast<ResultSet*>(cookie)->setRescode(lcb_respstore_status(dresp), key, nkey);
    } else {
        reinterpret_cast<ResultSet*>(cookie)->setRescode(LCB_ERR_GENERIC, key, nkey);
    }
}

static void cb_observe(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp)
{
    lcb_RESPSTORE *obresp = (lcb_RESPSTORE *)resp;
    void *cookie;
    int rflags;
    const char* key;
    size_t nkey;
    uint64_t cas;
    lcb_respstore_cookie(obresp, &cookie);
    rflags = lcb_respstore_observe_attached(obresp);
    lcb_respstore_key(obresp, &key, &nkey);
    lcb_respstore_cas(obresp, &cas);
    int ismaster;
    lcb_respstore_observe_master_exists(obresp, &ismaster);
    ResultSet *out = reinterpret_cast<ResultSet*>(cookie);

    if (lcb_respstore_status(obresp) == LCB_SUCCESS) {
        if (rflags & LCB_RESP_F_FINAL) {
            if (out->options.persist != out->obs_persist_count) {
                fprintf(stderr, "Item persistence not matched Received %d Expected %d \n",
                         out->obs_persist_count, out->options.persist);
            }
            if (out->options.replicate != out->obs_replica_count) {
                fprintf(stderr, "Item replication not matched Received %d Expected %d \n",
                        out->obs_replica_count, out->options.replicate);
            }
            out->setRescode(lcb_respstore_status(obresp), key, nkey);
        }
        if (ismaster == 1) {
            out->obs_persist_count++;
            out->obs_master_cas = cas;
            fprintf(stderr, "master cas %lu\n", (unsigned long)cas);
        }

        else if (lcb_respstore_status(obresp) == 1) {
            if (cas == out->obs_master_cas) {
                out->obs_persist_count++;
            } else {
                fprintf(stderr, "cas not matched master cas %lu  replica %lu \n",
                        (unsigned long)out->obs_master_cas, (unsigned long)cas);
            }
            out->obs_replica_count++;
        }
    } else {
        out->setRescode(lcb_respstore_status(obresp), key, nkey);
    }
}


static void cb_sd(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp)
{
    lcb_RESPSUBDOC *sdresp = (lcb_RESPSUBDOC *)(resp);
    void *cookie;
    lcb_respsubdoc_cookie(sdresp, &cookie);
    reinterpret_cast<ResultSet*>(cookie)->setRescode(lcb_respsubdoc_status(sdresp));
}

/**
 * see kv_engine/include/mcbp/protocol/status.h
 */
static const char * binary_status_codes[] = {
    "success (0x00)",
    "key_enoent (0x01)",
    "key_eexists (0x02)",
    "e2big (0x03)",
    "einval (0x04)",
    "not_stored (0x05)",
    "delta_badval (0x06)",
    "not_my_vbucket (0x07)",
    "no_bucket (0x08)",
    "locked (0x09)",
    "dcp_stream_not_found (0x0a)",
    "opaque_no_match (0x0b)",
    "unknown (0x0c)",
    "unknown (0x0d)",
    "unknown (0x0e)",
    "unknown (0x0f)",
    "unknown (0x10)",
    "unknown (0x11)",
    "unknown (0x12)",
    "unknown (0x13)",
    "unknown (0x14)",
    "unknown (0x15)",
    "unknown (0x16)",
    "unknown (0x17)",
    "unknown (0x18)",
    "unknown (0x19)",
    "unknown (0x1a)",
    "unknown (0x1b)",
    "unknown (0x1c)",
    "unknown (0x1d)",
    "unknown (0x1e)",
    "auth_stale (0x1f)",
    "auth_error (0x20)",
    "auth_continue (0x21)",
    "erange (0x22)",
    "rollback (0x23)",
    "eaccess (0x24)",
    "not_initialized (0x25)",
    "unknown (0x26)",
    "unknown (0x27)",
    "unknown (0x28)",
    "unknown (0x29)",
    "unknown (0x2a)",
    "unknown (0x2b)",
    "unknown (0x2c)",
    "unknown (0x2d)",
    "unknown (0x2e)",
    "unknown (0x2f)",
    "unknown (0x30)",
    "unknown (0x31)",
    "unknown (0x32)",
    "unknown (0x33)",
    "unknown (0x34)",
    "unknown (0x35)",
    "unknown (0x36)",
    "unknown (0x37)",
    "unknown (0x38)",
    "unknown (0x39)",
    "unknown (0x3a)",
    "unknown (0x3b)",
    "unknown (0x3c)",
    "unknown (0x3d)",
    "unknown (0x3e)",
    "unknown (0x3f)",
    "unknown (0x40)",
    "unknown (0x41)",
    "unknown (0x42)",
    "unknown (0x43)",
    "unknown (0x44)",
    "unknown (0x45)",
    "unknown (0x46)",
    "unknown (0x47)",
    "unknown (0x48)",
    "unknown (0x49)",
    "unknown (0x4a)",
    "unknown (0x4b)",
    "unknown (0x4c)",
    "unknown (0x4d)",
    "unknown (0x4e)",
    "unknown (0x4f)",
    "unknown (0x50)",
    "unknown (0x51)",
    "unknown (0x52)",
    "unknown (0x53)",
    "unknown (0x54)",
    "unknown (0x55)",
    "unknown (0x56)",
    "unknown (0x57)",
    "unknown (0x58)",
    "unknown (0x59)",
    "unknown (0x5a)",
    "unknown (0x5b)",
    "unknown (0x5c)",
    "unknown (0x5d)",
    "unknown (0x5e)",
    "unknown (0x5f)",
    "unknown (0x60)",
    "unknown (0x61)",
    "unknown (0x62)",
    "unknown (0x63)",
    "unknown (0x64)",
    "unknown (0x65)",
    "unknown (0x66)",
    "unknown (0x67)",
    "unknown (0x68)",
    "unknown (0x69)",
    "unknown (0x6a)",
    "unknown (0x6b)",
    "unknown (0x6c)",
    "unknown (0x6d)",
    "unknown (0x6e)",
    "unknown (0x6f)",
    "unknown (0x70)",
    "unknown (0x71)",
    "unknown (0x72)",
    "unknown (0x73)",
    "unknown (0x74)",
    "unknown (0x75)",
    "unknown (0x76)",
    "unknown (0x77)",
    "unknown (0x78)",
    "unknown (0x79)",
    "unknown (0x7a)",
    "unknown (0x7b)",
    "unknown (0x7c)",
    "unknown (0x7d)",
    "unknown (0x7e)",
    "unknown (0x7f)",
    "unknown_frame_info (0x80)",
    "unknown_command (0x81)",
    "enomem (0x82)",
    "not_supported (0x83)",
    "einternal (0x84)",
    "ebusy (0x85)",
    "etmpfail (0x86)",
    "xattr_einval (0x87)",
    "unknown_collection (0x88)",
    "unknown (0x89)",
    "cannot_apply_collections_manifest (0x8a)",
    "collections_manifest_is_ahead (0x8b)",
    "unknown_scope (0x8c)",
    "dcp_stream_id_invalid (0x8d)",
    "unknown (0x8e)",
    "unknown (0x8f)",
    "unknown (0x90)",
    "unknown (0x91)",
    "unknown (0x92)",
    "unknown (0x93)",
    "unknown (0x94)",
    "unknown (0x95)",
    "unknown (0x96)",
    "unknown (0x97)",
    "unknown (0x98)",
    "unknown (0x99)",
    "unknown (0x9a)",
    "unknown (0x9b)",
    "unknown (0x9c)",
    "unknown (0x9d)",
    "unknown (0x9e)",
    "unknown (0x9f)",
    "durability_invalid_level (0xa0)",
    "durability_impossible (0xa1)",
    "sync_write_in_progress (0xa2)",
    "sync_write_ambiguous (0xa3)",
    "sync_write_re_commit_in_progress (0xa4)",
    "unknown (0xa5)",
    "unknown (0xa6)",
    "unknown (0xa7)",
    "unknown (0xa8)",
    "unknown (0xa9)",
    "unknown (0xaa)",
    "unknown (0xab)",
    "unknown (0xac)",
    "unknown (0xad)",
    "unknown (0xae)",
    "unknown (0xaf)",
    "unknown (0xb0)",
    "unknown (0xb1)",
    "unknown (0xb2)",
    "unknown (0xb3)",
    "unknown (0xb4)",
    "unknown (0xb5)",
    "unknown (0xb6)",
    "unknown (0xb7)",
    "unknown (0xb8)",
    "unknown (0xb9)",
    "unknown (0xba)",
    "unknown (0xbb)",
    "unknown (0xbc)",
    "unknown (0xbd)",
    "unknown (0xbe)",
    "unknown (0xbf)",
    "subdoc_path_enoent (0xc0)",
    "subdoc_path_mismatch (0xc1)",
    "subdoc_path_einval (0xc2)",
    "subdoc_path_e2big (0xc3)",
    "subdoc_doc_e2deep (0xc4)",
    "subdoc_value_cantinsert (0xc5)",
    "subdoc_doc_not_json (0xc6)",
    "subdoc_num_erange (0xc7)",
    "subdoc_delta_einval (0xc8)",
    "subdoc_path_eexist (0xc9)",
    "subdoc_value_etoodeep (0xca)",
    "subdoc_invalid_combo (0xcb)",
    "subdoc_multi_path_failure (0xcc)",
    "subdoc_success_deleted (0xcd)",
    "subdoc_xattr_invalid_flag_combo (0xce)",
    "subdoc_xattr_invalid_key_combo (0xcf)",
    "subdoc_xattr_unknown_macro (0xd0)",
    "subdoc_xattr_unknown_vattr (0xd1)",
    "subdoc_xattr_cant_modify_vattr (0xd2)",
    "subdoc_multi_path_failure_deleted (0xd3)",
    "subdoc_invalid_xattr_order (0xd4)",
    "subdoc_xattr_unknown_vattr_macro (0xd5)",
    "unknown (0xd6)",
    "unknown (0xd7)",
    "unknown (0xd8)",
    "unknown (0xd9)",
    "unknown (0xda)",
    "unknown (0xdb)",
    "unknown (0xdc)",
    "unknown (0xdd)",
    "unknown (0xde)",
    "unknown (0xdf)",
    "unknown (0xe0)",
    "unknown (0xe1)",
    "unknown (0xe2)",
    "unknown (0xe3)",
    "unknown (0xe4)",
    "unknown (0xe5)",
    "unknown (0xe6)",
    "unknown (0xe7)",
    "unknown (0xe8)",
    "unknown (0xe9)",
    "unknown (0xea)",
    "unknown (0xeb)",
    "unknown (0xec)",
    "unknown (0xed)",
    "unknown (0xee)",
    "unknown (0xef)",
    "unknown (0xf0)",
    "unknown (0xf1)",
    "unknown (0xf2)",
    "unknown (0xf3)",
    "unknown (0xf4)",
    "unknown (0xf5)",
    "unknown (0xf6)",
    "unknown (0xf7)",
    "unknown (0xf8)",
    "unknown (0xf9)",
    "unknown (0xfa)",
    "unknown (0xfb)",
    "unknown (0xfc)",
    "unknown (0xfd)",
    "unknown (0xfe)",
    "unknown (0xff)",
};

const char *mc_code_to_str(uint16_t code)
{
  if (code > 0xff) {
    return "unknown (reserved)";
  }
  return binary_status_codes[code];
}

lcb_STATUS
lcb_errmap_user(lcb_INSTANCE *instance, uint16_t in)
{
    (void)instance;

    switch (in) {
        case PROTOCOL_BINARY_RESPONSE_NOT_MY_VBUCKET:
            return LCB_ERR_TIMEOUT;
        case PROTOCOL_BINARY_RESPONSE_AUTH_CONTINUE:
            return LCB_ERR_AUTH_CONTINUE;
        case PROTOCOL_BINARY_RESPONSE_EBUSY:
            return LCB_ERR_BUSY;
        case PROTOCOL_BINARY_RESPONSE_ETMPFAIL:
            return LCB_ERR_TEMPORARY_FAILURE;
        case PROTOCOL_BINARY_RESPONSE_EINTERNAL:
            return LCB_ERR_SDK_INTERNAL;
        default:
            fprintf(stderr, "Got unknown memcached error code: %d, %s\n",
                  in, mc_code_to_str(in));
            return LCB_ERR_GENERIC;
    }
}

static void wire_callbacks(lcb_INSTANCE *instance)
{
#define _setcb(t,cb) \
    lcb_install_callback(instance, t, cb)
    _setcb(LCB_CALLBACK_STORE, cb_storage);
    _setcb(LCB_CALLBACK_GET, cb_get);
    _setcb(LCB_CALLBACK_REMOVE, cb_remove);
    _setcb(LCB_CALLBACK_TOUCH, cb_touch);
    _setcb(LCB_CALLBACK_ENDURE, cb_endure);
    _setcb(LCB_CALLBACK_OBSERVE, cb_observe);
    _setcb(LCB_CALLBACK_GETREPLICA, cb_get);
    _setcb(LCB_CALLBACK_STOREDUR, cb_storedur);
    _setcb(LCB_CALLBACK_SDMUTATE, cb_sd);
    _setcb(LCB_CALLBACK_SDLOOKUP, cb_sd);
#undef _setcb
    lcb_set_errmap_callback(instance, lcb_errmap_user);
}

} /* extern "C" */


Handle::Handle(const HandleOptions& opts) :
        options(opts),
        instance(NULL)
{
}


Handle::~Handle() {

    if (instance != NULL) {
        lcb_destroy(instance);
    }

    if (logger != NULL) {
        delete logger;
    }

    if (io != NULL) {
        lcb_destroy_io_ops(io);
        io = NULL;
    }
    instance = NULL;
}

#define cstr_ornull(s) \
    ((s.size()) ? s.c_str() : NULL)
    static void open_callback(lcb_INSTANCE *instance, lcb_STATUS the_error)
    {
      if (the_error != LCB_SUCCESS) {
        char *bucketname = nullptr;
        lcb_cntl(instance, LCB_CNTL_GET, LCB_CNTL_BUCKETNAME, &bucketname);
        printf("open bucket \"%s\": %s\n", bucketname, lcb_strerror_short(the_error));
      }
    }


bool
Handle::connect(Error *errp)
{
    // Gather parameters
    lcb_STATUS the_error;
    lcb_CREATEOPTS *create_opts = NULL;
    instance = NULL;
    std::string connstr;
    logger = new Logger(Daemon::MainDaemon->getOptions().lcblogFile);

    if(options.useSSL) {
        connstr += std::string("couchbases://") + options.hostname;
        connstr += std::string("?certpath=");
        connstr += std::string(options.certpath);
        certpath = options.certpath;
        connstr += std::string("&");
    } else {
        connstr += std::string("couchbase://") + options.hostname;
        connstr += std::string("?");
    }
    connstr += "detailed_errcodes=1&";

    io = Daemon::MainDaemon->createIO();

    lcb_createopts_create(&create_opts, LCB_TYPE_CLUSTER);
    lcb_createopts_connstr(create_opts, connstr.c_str(), connstr.size());
    lcb_createopts_credentials(create_opts, options.username.c_str(), options.username.size(), options.password.c_str(), options.password.size());
    lcb_createopts_io(create_opts, io);
    lcb_createopts_logger(create_opts, logger->lcblogger);

    the_error = lcb_create(&instance, create_opts);
    if (the_error != LCB_SUCCESS) {
        errp->errstr = lcb_strerror_short(the_error);
        log_error("lcb_create failed: %s", errp->prettyPrint().c_str());
        abort();
        return false;
    }

    if (!instance) {
        errp->setCode(Error::SUBSYSf_CLIENT|Error::ERROR_GENERIC);
        errp->errstr = "Could not construct handle";
        return false;
    }

    if (Daemon::MainDaemon->getOptions().conncachePath) {
        char *path = Daemon::MainDaemon->getOptions().conncachePath;
        the_error = lcb_cntl(instance, LCB_CNTL_SET, LCB_CNTL_CONFIGCACHE, path);
    }
    int val= 1;
    the_error = lcb_cntl(instance, LCB_CNTL_SET, LCB_CNTL_ENABLE_MUTATION_TOKENS, &val);
    if (the_error != LCB_SUCCESS) {
        errp->errstr = lcb_strerror_short(the_error);
        log_error("lcb instance control settings failed: %s", errp->prettyPrint().c_str());
        return false;
    }
    the_error = lcb_cntl(instance, LCB_CNTL_SET, LCB_CNTL_DETAILED_ERRCODES, &val);
    if (the_error != LCB_SUCCESS) {
        errp->errstr = lcb_strerror_short(the_error);
        log_error("lcb instance control settings failed: %s", errp->prettyPrint().c_str());
        return false;
    }

    if (options.timeout) {
        unsigned long timeout = options.timeout * 1000000;
        the_error =lcb_cntl(instance, LCB_CNTL_SET, LCB_CNTL_OP_TIMEOUT, &timeout);
    }

    if (the_error != LCB_SUCCESS) {
        errp->errstr = lcb_strerror_short(the_error);
        log_error("lcb instance control settings failed: %s", errp->prettyPrint().c_str());
        return false;
    }
    the_error = lcb_connect(instance);
    if (the_error != LCB_SUCCESS) {
        errp->errstr = lcb_strerror_short(the_error);

        log_error("lcb_connect failed: %s", errp->prettyPrint().c_str());
        return false;
    }
    lcb_wait(instance, LCB_WAIT_NOCHECK);

    lcb_set_bootstrap_callback(instance, cb_config);
    lcb_set_cookie(instance, this);
    wire_callbacks(instance);

    the_error = lcb_get_bootstrap_status(instance);
    if (the_error != LCB_SUCCESS) {
        errp->errstr = lcb_strerror_short(the_error);

        log_error("lcb_bootstrap status failed: %s 0x%X", errp->prettyPrint().c_str(), the_error);
        return false;
    }


    if (pending_errors.size()) {
        *errp = pending_errors.back();
        pending_errors.clear();
        log_error("Got errors during connection");
        return false;
    }
    lcb_createopts_destroy(create_opts);

    lcb_set_open_callback(instance, open_callback);
    the_error = lcb_open(instance, options.bucket.c_str(), strlen(options.bucket.c_str()));
    lcb_wait(instance, LCB_WAIT_DEFAULT);
    if(the_error != LCB_SUCCESS){
        errp->errstr = lcb_strerror_short(the_error);
        log_error("Failed to open bucket: %s 0x%X", errp->prettyPrint().c_str(), the_error);
        return false;
    }

    return true;
}

bool Handle::generateCollections() {
    if(options.useCollections){
        log_info("Creating collections.\n");
        return collections->getInstance().generateCollections(instance, options.scopes, options.collections);
    }
    return false;
}

void
Handle::collect_result(ResultSet& rs)
{
    // Here we 'wait' for a result.. we might either wait after each
    // operation, or wait until we've accumulated all batches. It really
    // depends on the options.
    if (rs.remaining < 0) {
        log_error("Received extra callbacks");
    }
    if (!rs.remaining) {
        return;
    }
    lcb_wait(instance, LCB_WAIT_DEFAULT);
}

bool
Handle::postsubmit(ResultSet& rs, unsigned int nsubmit)
{
    rs.remaining += nsubmit;

    if (!rs.options.iterwait) {
        // everything is buffered up
        return true;
    }

    if (rs.remaining < rs.options.iterwait) {
        return true;
    }

    lcb_wait(instance, LCB_WAIT_DEFAULT);

    unsigned int wait_msec = rs.options.getDelay();

    if (wait_msec) {
        sdkd_millisleep(wait_msec);
    }

    if (Daemon::MainDaemon->getOptions().noPersist) {
        lcb_destroy(instance);
        Error e;
        connect(&e);
    }
    return false;
}

bool
Handle::dsGet(Command cmd, Dataset const &ds, ResultSet& out,
              const ResultOptions& options)
{
    out.options = options;
    out.clear();
    do_cancel = false;

    lcb_time_t exp = out.options.expiry;

    DatasetIterator* iter = ds.getIter();
    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();

        std::pair<string,string> collection = getCollection(k);

        lcb_CMDGET *cmd;
        lcb_cmdget_create(&cmd);
        if(collection.first.length() != 0) {
            lcb_cmdget_collection(cmd, collection.first.c_str(), collection.first.size(), collection.second.c_str(), collection.second.size());
        }
        lcb_cmdget_key(cmd, k.data(), k.size());
        lcb_cmdget_expiry(cmd, exp);

        out.markBegin();
        lcb_STATUS err = lcb_get(instance, &out, cmd);
        lcb_cmdget_destroy(cmd);

        if (err == LCB_SUCCESS) {
            postsubmit(out);
        } else {
            out.setRescode(err, k, true);
        }
    }

    delete iter;
    collect_result(out);
    return true;
}

bool
Handle::dsMutate(Command cmd, const Dataset& ds, ResultSet& out,
                 const ResultOptions& options)
{
    out.options = options;
    out.clear();
    lcb_STORE_OPERATION storop;
    do_cancel = false;

    if (cmd == Command::MC_DS_MUTATE_ADD) {
        storop = LCB_STORE_INSERT;
    } else if (cmd == Command::MC_DS_MUTATE_SET) {
        storop = LCB_STORE_UPSERT;
    } else if (cmd == Command::MC_DS_MUTATE_APPEND) {
        storop = LCB_STORE_APPEND;
    } else if (cmd == Command::MC_DS_MUTATE_PREPEND) {
        storop = LCB_STORE_PREPEND;
    } else if (cmd == Command::MC_DS_MUTATE_REPLACE) {
        storop = LCB_STORE_REPLACE;
    } else {
        out.oper_error = Error(Error::SUBSYSf_SDKD,
                               Error::SDKD_EINVAL,
                               "Unknown mutation operation");
        return false;
    }

    lcb_time_t exp = out.options.expiry;
    DatasetIterator *iter = ds.getIter();

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();
        std::string v = iter->value();

        std::pair<string,string> collection = getCollection(k);

        lcb_CMDSTORE *cmd;
        lcb_cmdstore_create(&cmd, storop);
        if(collection.first.length() != 0) {
            lcb_cmdstore_collection(cmd, collection.first.c_str(), collection.first.size(), collection.second.c_str(), collection.second.size());
        }
        lcb_cmdstore_key(cmd, k.data(), k.size());
        lcb_cmdstore_value(cmd, v.data(), v.size());
        lcb_cmdstore_expiry(cmd, exp);
        lcb_cmdstore_flags(cmd, FLAGS);


        out.markBegin();
        lcb_STATUS err = lcb_store(instance, &out, cmd);
        lcb_cmdstore_destroy(cmd);

        if (err == LCB_SUCCESS) {
            postsubmit(out);
        } else {
            out.setRescode(err, k, false);
        }
    }
    delete iter;
    collect_result(out);
    return true;
}

bool
Handle::dsGetReplica(Command cmd, Dataset const &ds, ResultSet& out,
              const ResultOptions& options)
{
    out.options = options;
    out.clear();
    do_cancel = false;

    DatasetIterator* iter = ds.getIter();

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();
        std::pair<string,string> collection = getCollection(k);
        log_trace("GET REPLICA : %s", k.c_str());

        lcb_CMDGETREPLICA *cmd;
        lcb_cmdgetreplica_create(&cmd, LCB_REPLICA_MODE_ANY);
        if(collection.first.length() != 0) {
            lcb_cmdgetreplica_collection(cmd, collection.first.c_str(), collection.first.size(), collection.second.c_str(), collection.second.size());;
        }
        lcb_cmdgetreplica_key(cmd, k.data(), k.size());

        out.markBegin();
        lcb_STATUS err = lcb_getreplica(instance, &out, cmd);
        lcb_cmdgetreplica_destroy(cmd);

        if (err == LCB_SUCCESS) {
            postsubmit(out);
        } else {
            out.setRescode(err, k, true);
        }
    }

    delete iter;
    collect_result(out);
    return true;
}

bool
Handle::dsEndure(Command cmd, Dataset const &ds, ResultSet& out,
        const ResultOptions& options)
{
    out.options = options;
    out.clear();
    do_cancel = false;

    DatasetIterator* iter = ds.getIter();

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key(), v = iter->value();
        std::pair<string,string> collection = getCollection(k);

        lcb_CMDSTORE *cmd;
        lcb_cmdstore_create(&cmd, LCB_STORE_UPSERT);
        if(collection.first.length() != 0) {
            lcb_cmdstore_collection(cmd, collection.first.c_str(), collection.first.size(), collection.second.c_str(), collection.second.size());
        }
        lcb_cmdstore_key(cmd, k.data(), k.size());
        lcb_cmdstore_value(cmd, v.data(), v.size());
        lcb_cmdstore_durability_observe(cmd, options.persist, options.replicate);

        out.markBegin();

        lcb_STATUS err = lcb_store(instance, &out, cmd);
        lcb_cmdstore_destroy(cmd);

        if (err == LCB_SUCCESS) {
            postsubmit(out);
        } else {
            out.setRescode(err, k, true);
        }
    }

    delete iter;
    collect_result(out);
    return true;
}

bool
Handle::dsObserve(Command cmd, Dataset const &ds, ResultSet& out,
              const ResultOptions& options)
{
    out.options = options;
    out.clear();
    do_cancel = false;

    DatasetIterator* iter = ds.getIter();

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();
        std::pair<string,string> collection = getCollection(k);

        lcb_CMDSTORE *cmd;
        lcb_cmdstore_create(&cmd, LCB_STORE_UPSERT);
        if(collection.first.length() != 0) {
            lcb_cmdstore_collection(cmd, collection.first.c_str(), collection.first.size(), collection.second.c_str(), collection.second.size());
        }
        lcb_cmdstore_key(cmd, k.c_str(), k.size());

        out.markBegin();

        lcb_STATUS err = lcb_store(instance, &out, cmd);
        lcb_cmdstore_destroy(cmd);

        out.obs_persist_count = 0;
        out.obs_replica_count = 0;
        if (err == LCB_SUCCESS) {
            postsubmit(out);
        } else {
            out.setRescode(err, k, true);
        }
    }

    delete iter;
    collect_result(out);
    return true;
}



bool
Handle::dsKeyop(Command cmd, const Dataset& ds, ResultSet& out,
                const ResultOptions& options)
{
    out.options = options;
    out.clear();
    DatasetIterator *iter = ds.getIter();
    do_cancel = false;

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();
        std::pair<string,string> collection = getCollection(k);
        lcb_STATUS err;

        out.markBegin();

        if (cmd == Command::MC_DS_DELETE) {
            lcb_CMDREMOVE *cmd;
            lcb_cmdremove_create(&cmd);
            lcb_cmdremove_key(cmd, k.data(), k.size());
            err = lcb_remove(instance, &out, cmd);
            lcb_cmdremove_destroy(cmd);
        } else {
            lcb_CMDTOUCH *cmd;
            lcb_cmdtouch_create(&cmd);
            lcb_cmdtouch_key(cmd, k.data(), k.size());
            err = lcb_touch(instance, &out, cmd);
            lcb_cmdtouch_destroy(cmd);
        }

        if (err == LCB_SUCCESS) {
            postsubmit(out);
        } else {
            out.setRescode(err, k, false);
        }
    }
    delete iter;
    collect_result(out);
    return true;
}



bool
Handle::dsSDSinglePath(Command c, const Dataset& ds, ResultSet& out,
        const ResultOptions& options) {

    out.options = options;
    out.clear();
    DatasetIterator *iter = ds.getIter();
    do_cancel = false;
    lcb_SUBDOCSPECS *op;

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string key = iter->key();
        std::string path = iter->path();
        std::string value = iter->value();
        std::string command = iter->command();
        std::pair<string,string> collection = getCollection(key);

        if(command == "get_multi"){
            lcb_subdocspecs_create(&op, 2);
        }else{
            lcb_subdocspecs_create(&op, 1);
        }

        lcb_CMDSUBDOC *cmd;
        lcb_cmdsubdoc_create(&cmd);
        if(collection.first.length() != 0) {
            lcb_cmdsubdoc_collection(cmd, collection.first.c_str(), collection.first.size(), collection.second.c_str(), collection.second.size());
        }

        if (command == "get") {
            lcb_subdocspecs_get(op, 0, 0, path.c_str(), path.size());
        } else if (command == "get_multi") {
            lcb_subdocspecs_get(op, 0, 0, path.c_str(), path.size());
            lcb_subdocspecs_exists(op, 1, 0, path.c_str(), path.size());
        }else if (command == "replace") {
            lcb_subdocspecs_replace(op, 0, 0, path.c_str(), path.size(), value.c_str(), value.size());
        } else if (command == "dict_add") {
            lcb_subdocspecs_dict_add(op, 0, 0, path.c_str(), path.size(), value.c_str(), value.size());
        } else if (command == "dict_upsert") {
            lcb_subdocspecs_dict_upsert(op, 0, 0, path.c_str(), path.size(), value.c_str(), value.size());
        } else if (command == "array_add") {
            lcb_subdocspecs_array_add_first(op, 0, 0, path.c_str(), path.size(), value.c_str(), value.size());
        } else if (command == "array_add_last") {
            lcb_subdocspecs_array_add_last(op, 0, 0, path.c_str(), path.size(), value.c_str(), value.size());
        } else if (command == "counter") {
            lcb_subdocspecs_counter(op, 0, 0, path.c_str(), path.size(), 0);
        }
        lcb_cmdsubdoc_key(cmd, key.c_str(), key.size());

        lcb_cmdsubdoc_specs(cmd, op);

        out.markBegin();

        lcb_STATUS err =  lcb_subdoc(instance, &out, cmd);
        lcb_cmdsubdoc_destroy(cmd);

        if (err == LCB_SUCCESS) {
            postsubmit(out);
        } else {
            out.setRescode(err, key, true);
        }
    }

    lcb_subdocspecs_destroy(op);
    delete iter;
    collect_result(out);
    return true;
}

void
Handle::cancelCurrent()
{
    do_cancel = true;
    //delete certfile if exists
    if (certpath.size()) {
        remove(certpath.c_str());
    }
}

//Get scope name and collection name to use from the key
//We expect keys with a trailing numeric part "SimpleKeyREP11155REP11155REP11155", "key5", "24", etc.
std::pair<string, string> Handle::getCollection(const std::string key) {
    std::pair<string, string> coll("", "");//Converts to default collection
    if(options.useCollections && !key.empty()){
        //Defaults
        coll.first = "0";
        coll.second = "0";
        std::string last_n = key.substr(max(0, (int)key.length() - 3));//Last 3 chars or whole string
        int key_num = std::stoi(std::regex_replace(last_n, std::regex(R"([\D])"), ""));//Remove any remaining non-numeric chars
        int collection_num  = key_num % (options.collections * options.scopes);
        int scope_num = floor((float)collection_num / (float)options.collections);

        coll.first  = to_string(scope_num);
        coll.second = to_string(collection_num);
    }
    return coll;
}

} /* namespace CBSdkd */
