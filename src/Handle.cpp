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
    fprintf(stderr, " SDK version changeset %s", hdrComponents["CHANGESET"].asString().c_str());
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

lcb_STATUS
lcb_errmap_user(lcb_INSTANCE *instance, lcb_uint16_t in)
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
            fprintf(stderr, "Got unknown error code %d \n", in);
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
        printf("open bucket: %s\n", lcb_strerror_short(the_error));
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
