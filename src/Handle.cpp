/*
 * Handle.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include "Handle.h"
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

namespace CBSdkd {

std::map<libcouchbase_error_t,int> Handle::Errmap =
        create_map<libcouchbase_error_t,int>
#define X(a,b) (a,b)
CBSDKD_XERRMAP(X);
#undef X


extern "C" {
static void cb_err(libcouchbase_t instance,
               libcouchbase_error_t err, const char *desc)
{
    Handle *handle = (Handle*)libcouchbase_get_cookie(instance);
    int myerr = Handle::mapError(err,
                                 Error::SUBSYSf_CLIENT|Error::SUBSYSf_NETWORK);
    handle->appendError(myerr, desc);
}

#ifdef LCB_VERSION
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

#else /* !LCB_VERSION */

static void cb_keyop(libcouchbase_t instance, void* rs,
                 libcouchbase_error_t err,
                 const void* key, libcouchbase_size_t nkey)
{
    reinterpret_cast<ResultSet*>(rs)->setRescode(err, key, nkey);
}

static void cb_storage(libcouchbase_t instance, ResultSet* rs,
                   libcouchbase_storage_t storop,
                   libcouchbase_error_t err,
                   const void* key, libcouchbase_size_t nkey,
                   libcouchbase_cas_t cas)
{
    reinterpret_cast<ResultSet*>(rs)->setRescode(err, key, nkey);
}

static void cb_get(libcouchbase_t instance, ResultSet* rs,
               libcouchbase_error_t err,
               const void *key, libcouchbase_size_t nkey,
               const void *value, libcouchbase_size_t nvalue,
               libcouchbase_uint32_t flags, libcouchbase_cas_t cas)
{
    reinterpret_cast<ResultSet*>(rs)->
            setRescode(err, key, nkey, true, value, nvalue);
}

static void wire_callbacks(libcouchbase_t instance)
{
#define _set_cb(bname, cb) \
    libcouchbase_set_##bname##_callback(instance, \
                                        (libcouchbase_##bname##_callback)cb)

    _set_cb(touch, cb_keyop);
    _set_cb(remove, cb_keyop);
    _set_cb(storage, cb_storage);
    _set_cb(get, cb_get);
#undef _set_cb
}
#endif /* !LCB_VERSION */

} /* extern "C" */


Handle::Handle(const HandleOptions& opts) :
        options(opts),
        is_connected(false),
        instance(NULL)
{
}

Handle::~Handle() {
    if (instance != NULL) {
        libcouchbase_destroy(instance);
    }
    instance = NULL;
}

#define cstr_ornull(s) \
    ((s.size()) ? s.c_str() : NULL)



bool
Handle::connect(Error *errp)
{
    // Gather parameters
    libcouchbase_error_t the_error;
    log_debug("Using %s as hostname", options.hostname.c_str());

    instance = libcouchbase_create(cstr_ornull(options.hostname),
                                   cstr_ornull(options.username),
                                   cstr_ornull(options.password),
                                   cstr_ornull(options.bucket),
                                   NULL);

    if (!instance) {
        errp->setCode(Error::SUBSYSf_CLIENT|Error::ERROR_GENERIC);
        errp->errstr = "Could not construct handle";
        return false;
    }

    if (options.timeout) {
        log_debug("Setting timeout to %d sec", options.timeout);
        libcouchbase_set_timeout(instance, options.timeout * 1000000);
    }

    libcouchbase_set_error_callback(instance, cb_err);
    libcouchbase_set_cookie(instance, this);
    wire_callbacks(instance);

    the_error = libcouchbase_connect(instance);
    if (the_error != LIBCOUCHBASE_SUCCESS) {
        errp->setCode(mapError(the_error,
                               Error::SUBSYSf_NETWORK|Error::ERROR_GENERIC));
        errp->errstr = libcouchbase_strerror(instance, the_error);

        log_error("libcouchbase_connect failed: %s",
                  errp->prettyPrint().c_str());
        return false;
    }

    libcouchbase_wait(instance);
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
    libcouchbase_wait(instance);
}

void
Handle::postsubmit(ResultSet& rs, unsigned int nsubmit)
{
    unsigned int wait_msec = rs.options.getDelay();
    // So this is called after each 'submission' for a command to
    // libcouchbase. In here we can either do nothing (batch single/multi).
    if (wait_msec || rs.options.iterwait) {
        libcouchbase_wait(instance);
    } else {
        rs.remaining += nsubmit;
        return;
    }

    if (wait_msec) {
        usleep(wait_msec * 1000);
    }
}

bool
Handle::dsGet(Command cmd, Dataset const &ds, ResultSet& out,
              const ResultOptions& options)
{
    out.options = options;
    out.clear();
    do_cancel = false;

    libcouchbase_time_t exp = out.options.expiry;
    libcouchbase_time_t *exp_pp = (exp) ? &exp : NULL;

    DatasetIterator* iter = ds.getIter();
    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();
        log_trace("GET: %s", k.c_str());
        libcouchbase_size_t sz = k.size();
        const char *cstr = k.c_str();

        out.markBegin();

        libcouchbase_error_t err =
                libcouchbase_mget(instance, &out, 1,
                                  (const void**)&cstr, &sz, exp_pp);
        if (err == LIBCOUCHBASE_SUCCESS) {
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
    libcouchbase_storage_t storop;
    do_cancel = false;

    if (cmd == Command::MC_DS_MUTATE_ADD) {
        storop = LIBCOUCHBASE_ADD;
    } else if (cmd == Command::MC_DS_MUTATE_SET) {
        storop = LIBCOUCHBASE_SET;
    } else if (cmd == Command::MC_DS_MUTATE_APPEND) {
        storop = LIBCOUCHBASE_APPEND;
    } else if (cmd == Command::MC_DS_MUTATE_PREPEND) {
        storop = LIBCOUCHBASE_PREPEND;
    } else if (cmd == Command::MC_DS_MUTATE_REPLACE) {
        storop = LIBCOUCHBASE_REPLACE;
    } else {
        out.oper_error = Error(Error::SUBSYSf_SDKD,
                               Error::SDKD_EINVAL,
                               "Unknown mutation operation");
        return false;
    }

    libcouchbase_time_t exp = out.options.expiry;
    DatasetIterator *iter = ds.getIter();

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key(), v = iter->value();
        libcouchbase_size_t ksz = k.size(), vsz = v.size();
        const char *kstr = k.data(), *vstr = v.data();

        out.markBegin();
        libcouchbase_error_t err =
                libcouchbase_store(instance, &out, storop,
                                   kstr, ksz,
                                   vstr, vsz,
                                   0, exp, 0);

        if (err == LIBCOUCHBASE_SUCCESS) {
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
    libcouchbase_time_t exp = out.options.expiry;
    DatasetIterator *iter = ds.getIter();
    do_cancel = false;

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();
        const char *kstr = k.data();
        libcouchbase_size_t ksz = k.size();
        libcouchbase_error_t err;

        out.markBegin();

        if (cmd == Command::MC_DS_DELETE) {
            err = libcouchbase_remove(instance, &out, kstr, ksz, 0);
        } else {
            err = libcouchbase_mtouch(instance, &out,
                                      1, (const void**)&kstr, &ksz,
                                      &exp);
        }
        if (err == LIBCOUCHBASE_SUCCESS) {
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
