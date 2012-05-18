/*
 * Handle.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include "Handle.h"
#include <cstdlib>
#include <cstdio>


namespace CBSdkd {

std::map<libcouchbase_error_t,int> Handle::Errmap =
        create_map<libcouchbase_error_t,int>
#define X(a,b) (a,b)
CBSDKD_XERRMAP(X);
#undef X

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
//        libcouchbase_set_timeout(instance, options.timeout * 1000000);
    }

    libcouchbase_set_error_callback(instance, cb_err);

    libcouchbase_set_touch_callback(instance,
                                    (libcouchbase_touch_callback)cb_keyop);
    libcouchbase_set_remove_callback(instance,
                                     (libcouchbase_remove_callback)cb_keyop);

    libcouchbase_set_storage_callback(instance,
                                      (libcouchbase_storage_callback)cb_storage);

    libcouchbase_set_get_callback(instance,
                                  (libcouchbase_get_callback)cb_get);

    libcouchbase_set_cookie(instance, this);

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
Handle::cb_err(libcouchbase_t instance,
               libcouchbase_error_t err, const char *desc)
{
    Handle *handle = (Handle*)libcouchbase_get_cookie(instance);
    int myerr = mapError(err, Error::SUBSYSf_CLIENT|Error::SUBSYSf_NETWORK);
    handle->pending_errors.push_back(Error(myerr, desc));
}

static void
_kop_common(ResultSet *rs,
            const void *key, size_t nkey, libcouchbase_error_t err)
{
    int myerr = Handle::mapError(err, Error::SUBSYSf_MEMD|Error::ERROR_GENERIC);
    rs->stats[myerr]++;
    rs->remaining--;
    if (rs->options.full) {
        std::string str;
        str.assign((const char*)key, nkey);
        rs->fullstats[str] = myerr;
    }
}

void
Handle::cb_keyop(libcouchbase_t instance, ResultSet* rs,
                 libcouchbase_error_t err,
                 const void* key, libcouchbase_size_t nkey)
{
    _kop_common(rs, key, nkey, err);
}

void
Handle::cb_storage(libcouchbase_t instance, ResultSet* rs,
                   libcouchbase_storage_t storop,
                   libcouchbase_error_t err,
                   const void* key, libcouchbase_size_t nkey,
                   libcouchbase_cas_t cas)
{
    _kop_common(rs, key, nkey, err);
}

void
Handle::cb_get(libcouchbase_t instance, ResultSet* rs,
               libcouchbase_error_t err,
               const void *key, libcouchbase_size_t nkey,
               const void *value, libcouchbase_size_t nvalue,
               libcouchbase_uint32_t flags, libcouchbase_cas_t cas)
{
    int myerr = mapError(err, Error::SUBSYSf_MEMD|Error::ERROR_GENERIC);
    rs->stats[myerr]++;
    if (rs->options.full) {
        std::string sk((const char*)key, nkey);
        std::string sv;
        if (nvalue) {
            sv.assign((const char*)value, nvalue);
        } else {
            sv = "";
        }
        rs->fullstats[sk] = sv;
    }
    rs->remaining--;
}

void
ResultSet::setError(libcouchbase_t instance, libcouchbase_error_t err,
                    const std::string& key)
{
    int myerr = Handle::mapError(err,
                                 Error::SUBSYSf_CLIENT|Error::ERROR_GENERIC);
    stats[myerr]++;
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
    unsigned int expmsec = rs.options.getDelay();
    // So this is called after each 'submission' for a command to
    // libcouchbase. In here we can either do nothing (batch single/multi).
    if (expmsec || rs.options.iterwait) {
        libcouchbase_wait(instance);
    } else {
        rs.remaining += nsubmit;
        return;
    }

    if (expmsec) {
        usleep(expmsec * 1000);
    }
}

bool
Handle::dsGet(Command cmd, Dataset const &ds, ResultSet& out,
              const ResultOptions& options)
{
    out.options = options;
    out.clear();

    libcouchbase_time_t exp = out.options.expiry;
    libcouchbase_time_t *exp_pp = (exp) ? &exp : NULL;

    DatasetIterator* iter = ds.getIter();
    for (iter->start(); iter->done() == false; iter->advance()) {
        std::string k = iter->key();
        log_trace("GET: %s", k.c_str());
        libcouchbase_size_t sz = k.size();
        const char *cstr = k.c_str();

        libcouchbase_error_t err =
                libcouchbase_mget(instance, &out, 1,
                                  (const void**)&cstr, &sz, exp_pp);
        if (err == LIBCOUCHBASE_SUCCESS) {
            postsubmit(out);

        } else {
            out.setError(instance, err, k);
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


    for (iter->start(); iter->done() == false; iter->advance()) {
        std::string k = iter->key(), v = iter->value();
        libcouchbase_size_t ksz = k.size(), vsz = v.size();
        const char *kstr = k.data(), *vstr = v.data();

        libcouchbase_error_t err =
                libcouchbase_store(instance, &out, storop,
                                   kstr, ksz,
                                   vstr, vsz,
                                   0, exp, 0);

        if (err == LIBCOUCHBASE_SUCCESS) {
            postsubmit(out);
        } else {
            out.setError(instance, err, k);
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


    for (iter->start(); iter->done() == false; iter->advance()) {
        std::string k = iter->key();
        const char *kstr = k.data();
        libcouchbase_size_t ksz = k.size();
        libcouchbase_error_t err;
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
            out.setError(instance, err, k);
        }
    }
    delete iter;
    collect_result(out);
    return true;
}

void
ResultSet::resultsJson(Json::Value *in) const
{
    Json::Value
        summaries = Json::Value(Json::objectValue),
        &root = *in;

    for (std::map<int,int>::const_iterator iter = this->stats.begin();
            iter != this->stats.end(); iter++ ) {
        stringstream ss;
        ss << iter->first;
        summaries[ss.str()] = iter->second;
    }

    root[CBSDKD_MSGFLD_DSRES_STATS] = summaries;

    if (options.full) {
        Json::Value fullstats;
        for (
                std::map<std::string,FullResult>::const_iterator
                    iter = this->fullstats.begin();
                iter != this->fullstats.end();
                iter++
                )
        {
            Json::Value stat;
            stat[0] = iter->second.getStatus();
            stat[1] = iter->second.getString();
            fullstats[iter->first] = stat;
        }
        root[CBSDKD_MSGFLD_DSRES_FULL] = fullstats;
    }
}

} /* namespace CBSdkd */
