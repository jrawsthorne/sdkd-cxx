/*
 * Handle.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

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
    rtComponents["SDK"] = vbuf;

    sprintf(vbuf, "0x%x", LCB_VERSION);
    hdrComponents["SDK"] = vbuf;

// Thanks mauke
#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)
    hdrComponents["CHANGESET"] = STRINGIFY(LCB_VERSION_CHANGESET);
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
}



static void cb_err(lcb_t instance, lcb_error_t err, const char *desc)
{
    Handle *handle = (Handle*)lcb_get_cookie(instance);
    int myerr = Handle::mapError(err);
    handle->appendError(myerr, desc ? desc : "");
    log_noctx_error("Got error %d: %s", err, desc ? desc : "");
}

static void cb_config(lcb_t instance, lcb_configuration_t config)
{
    (void)instance;
    (void)config;
    // Too verbose
    //log_noctx_trace("Instance %p: CONFIG UPDATE [%d]", instance, config);
}

static void cb_remove(lcb_t instance, void *rs,
                     lcb_error_t err, const lcb_remove_resp_t *resp)
{
    reinterpret_cast<ResultSet*>(rs)->setRescode(err,
            resp->v.v0.key, resp->v.v0.nkey);
}

static void cb_touch(lcb_t instance, void *rs,
                     lcb_error_t err, const lcb_touch_resp_t *resp)
{
    reinterpret_cast<ResultSet*>(rs)->setRescode(err,
            resp->v.v0.key, resp->v.v0.nkey);
}

static void cb_storage(lcb_t instance, void *rs,
                       lcb_storage_t oper, lcb_error_t err,
                       const lcb_store_resp_t *resp)
{
    reinterpret_cast<ResultSet*>(rs)->setRescode(err,
            resp->v.v0.key, resp->v.v0.nkey);
}

static void cb_get(lcb_t instance, void *rs,
                   lcb_error_t err, const lcb_get_resp_t *resp)
{
    reinterpret_cast<ResultSet*>(rs)->setRescode(err,
            resp->v.v0.key, resp->v.v0.nkey, true,
            resp->v.v0.bytes, resp->v.v0.nbytes);
}

static void wire_callbacks(lcb_t instance)
{
#define _setcb(t,cb) \
    lcb_set_##t##_callback(instance,(lcb_##t##_callback)cb)
    _setcb(store, cb_storage);
    _setcb(get, cb_get);
    _setcb(remove, cb_remove);
    _setcb(touch, cb_touch);
#undef _setcb
}

} /* extern "C" */


Handle::Handle(const HandleOptions& opts) :
        options(opts),
        is_connected(false),
        instance(NULL)
{
    create_opts.version = 0;
}

Handle::~Handle() {
    if (instance != NULL) {
        lcb_destroy(instance);
    }

    if (io != NULL) {
        lcb_destroy_io_ops(io);
        io = NULL;
    }
    instance = NULL;
}

#define cstr_ornull(s) \
    ((s.size()) ? s.c_str() : NULL)



bool
Handle::connect(Error *errp)
{
    // Gather parameters
    lcb_error_t the_error;
    instance = NULL;

    if (!create_opts.v.v0.host) {
        create_opts.v.v0.host = cstr_ornull(options.hostname);
        create_opts.v.v0.user = cstr_ornull(options.username);
        create_opts.v.v0.passwd = cstr_ornull(options.password);
        create_opts.v.v0.bucket = cstr_ornull(options.bucket);
        if (Daemon::MainDaemon->getOptions().conncachePath) {
            memset(&cached_opts, 0, sizeof(cached_opts));
            memcpy(&cached_opts.createopt, &create_opts, sizeof(create_opts));
            cached_opts.cachefile = Daemon::MainDaemon->getOptions().conncachePath;
        }
    }

    io = Daemon::MainDaemon->createIO();
    create_opts.v.v0.io = io;

    if (Daemon::MainDaemon->getOptions().conncachePath) {
        the_error = lcb_create_compat(LCB_CACHED_CONFIG, &cached_opts, &instance, NULL);
    } else {
        the_error = lcb_create(&instance, &create_opts);
    }

    if (!instance) {
        errp->setCode(Error::SUBSYSf_CLIENT|Error::ERROR_GENERIC);
        errp->errstr = "Could not construct handle";
        return false;
    }

    if (options.timeout) {
        lcb_set_timeout(instance, options.timeout * 1000000);
    }

    lcb_set_error_callback(instance, cb_err);
    lcb_set_configuration_callback(instance, cb_config);
    lcb_set_cookie(instance, this);
    wire_callbacks(instance);

    the_error = lcb_connect(instance);
    if (the_error != LCB_SUCCESS) {
        errp->setCode(mapError(the_error));
        errp->errstr = lcb_strerror(instance, the_error);

        log_error("lcb_connect failed: %s", errp->prettyPrint().c_str());
        return false;
    }

    lcb_wait(instance);
    if (pending_errors.size()) {
        *errp = pending_errors.back();
        pending_errors.clear();
        log_error("Got errors during connection");
        return false;
    }
    return true;
}

void
Handle::collect_result(ResultSet& rs)
{
    // Here we 'wait' for a result.. we might either wait after each
    // operation, or wait until we've accumulated all batches. It really
    // depends on the options.
    if (!rs.remaining) {
        return;
    }
    lcb_wait(instance);
}

void
Handle::postsubmit(ResultSet& rs, unsigned int nsubmit)
{

    rs.remaining += nsubmit;

    if (!rs.options.iterwait) {
        // everything is buffered up
        return;
    }

    if (rs.remaining < rs.options.iterwait) {
        return;
    }

    lcb_wait(instance);

    unsigned int wait_msec = rs.options.getDelay();

    if (wait_msec) {
        sdkd_millisleep(wait_msec);
    }

    if (Daemon::MainDaemon->getOptions().noPersist) {
        lcb_destroy(instance);
        Error e;
        connect(&e);
    }
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
        log_trace("GET: %s", k.c_str());

        lcb_get_cmd_t gcmd;
        const lcb_get_cmd_t *cmdp = &gcmd;

        gcmd.v.v0.key = k.data();
        gcmd.v.v0.nkey = k.size();
        gcmd.v.v0.exptime = exp;

        out.markBegin();

        lcb_error_t err = lcb_get(instance, &out, 1, &cmdp);

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
    lcb_storage_t storop;
    do_cancel = false;

    if (cmd == Command::MC_DS_MUTATE_ADD) {
        storop = LCB_ADD;
    } else if (cmd == Command::MC_DS_MUTATE_SET) {
        storop = LCB_SET;
    } else if (cmd == Command::MC_DS_MUTATE_APPEND) {
        storop = LCB_APPEND;
    } else if (cmd == Command::MC_DS_MUTATE_PREPEND) {
        storop = LCB_PREPEND;
    } else if (cmd == Command::MC_DS_MUTATE_REPLACE) {
        storop = LCB_REPLACE;
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

        std::string k = iter->key(), v = iter->value();

        lcb_store_cmd_t scmd;
        const lcb_store_cmd_t *cmdp = &scmd;

        scmd.v.v0.key = k.data();
        scmd.v.v0.nkey = k.size();

        scmd.v.v0.bytes = v.data();
        scmd.v.v0.nbytes = v.size();

        scmd.v.v0.exptime = exp;
        scmd.v.v0.operation = storop;

        out.markBegin();
        lcb_error_t err = lcb_store(instance, &out, 1, &cmdp);

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
        log_trace("GET REPLICA : %s", k.c_str());

        lcb_get_replica_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));

        cmd.v.v1.key = k.data();
        cmd.v.v1.nkey = k.size();
        cmd.v.v1.strategy = LCB_REPLICA_ALL;

        const lcb_get_replica_cmd_t *cmds[] = { &cmd };

        out.markBegin();
        lcb_error_t err = lcb_get_replica(instance, &out, 1, cmds);

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
    lcb_time_t exp = out.options.expiry;
    DatasetIterator *iter = ds.getIter();
    do_cancel = false;

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();
        lcb_error_t err;

        out.markBegin();

        if (cmd == Command::MC_DS_DELETE) {
            lcb_remove_cmd_t rmcmd = lcb_remove_cmd_st(k.data(), k.size());
            const lcb_remove_cmd_t *cmdp = &rmcmd;
            err = lcb_remove(instance, &out, 1, &cmdp);

        } else {
            lcb_touch_cmd_t tcmd = lcb_touch_cmd_t(k.data(), k.size(), exp);
            const lcb_touch_cmd_t *cmdp = &tcmd;
            err = lcb_touch(instance, &out, 1, &cmdp);

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

void
Handle::cancelCurrent()
{
    do_cancel = true;
}


} /* namespace CBSdkd */
