/*
 * Handle.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include "sdkd_internal.h"

namespace CBSdkd {

extern "C" {

/* Declared as extern */
const char *SDKD_Conncache_Path = NULL;
int SDKD_No_Persist = 0;

static void cb_err(lcb_t instance, lcb_error_t err, const char *desc)
{
    Handle *handle = (Handle*)lcb_get_cookie(instance);
    int myerr = Handle::mapError(err,
                                 Error::SUBSYSf_CLIENT|Error::SUBSYSf_NETWORK);
    handle->appendError(myerr, desc ? desc : "");
    fprintf(stderr, "Got error %d: %s\n", err, desc ? desc : "");
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
}

Handle::~Handle() {
    if (instance != NULL) {
        lcb_destroy(instance);
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
        if (SDKD_Conncache_Path) {
            memset(&cached_opts, 0, sizeof(cached_opts));
            memcpy(&cached_opts.createopt, &create_opts, sizeof(create_opts));
            cached_opts.cachefile = SDKD_Conncache_Path;
        }
    }

    create_opts.v.v0.io = sdkd_create_iops();

    if (SDKD_Conncache_Path) {
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
    lcb_set_cookie(instance, this);
    wire_callbacks(instance);

    the_error = lcb_connect(instance);
    if (the_error != LCB_SUCCESS) {
        errp->setCode(mapError(the_error,
                               Error::SUBSYSf_NETWORK|Error::ERROR_GENERIC));
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

    if (SDKD_No_Persist) {
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
