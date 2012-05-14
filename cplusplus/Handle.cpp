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
        libcouchbase_set_timeout(instance, options.timeout * 1000000);
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
        errp->setCode(mapError(the_error));
        return false;
    }

    libcouchbase_wait(instance);
    if (pending_errors.size()) {
        *errp = pending_errors.back();
        pending_errors.clear();
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

    if (rs->options.delay) {
        usleep(rs->options.delay * 1000);
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

bool
Handle::dsGet(Command cmd, Dataset const &ds, ResultSet& out,
              const ResultOptions& options)
{

    out.options = options;
    out.clear();
    libcouchbase_time_t exp = out.options.expiry;
    DatasetIterator* iter = ds.getIter();

    for (iter->start(); iter->done() == false; iter->advance()) {

        std::string k = iter->key();
        libcouchbase_size_t sz = k.size();
        const char *cstr = k.c_str();

        libcouchbase_error_t err =
                libcouchbase_mget(instance, &out, 1,
                                  (const void**)&cstr, &sz, &exp);
        if (err == LIBCOUCHBASE_SUCCESS) {
            out.remaining++;
        } else {
            out.setError(instance, err, k);
        }
    }

    delete iter;

    if (out.remaining) {
        libcouchbase_wait(instance);
    }

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
            out.remaining++;
        } else {
            out.setError(instance, err, k);
        }
        if (out.options.delay) {
            usleep(out.options.delay * 1000);
        }
    }
    delete iter;

    if (out.remaining) {
        libcouchbase_wait(instance);
    }
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
            out.remaining++;
        } else {
            out.setError(instance, err, k);
        }
        if (out.options.delay) {
            usleep(out.options.delay * 1000);
        }
    }
    delete iter;

    if (out.remaining) {
        libcouchbase_wait(instance);
    }
    return true;
}

void
ResultSet::resultsJson(Json::Value *in) const
{
    Json::Value summaries, &root = *in;

    for (std::map<int,int>::const_iterator iter = this->stats.begin();
            iter != this->stats.end(); iter++ ) {
        summaries[iter->first] = iter->second;
    }

    root[CBSDKD_MSGFLD_DSRES_STATS] = summaries;

    if (options.full) {
        Json::Value fullstats;
        for (
                std::map<std::string,FullResult>::const_iterator
                    iter = this->fullstats.begin();
                iter != this->fullstats.end();
                iter++
                ) {
            Json::Value stat;
            stat[0] = iter->second.getStatus();
            stat[1] = iter->second.getString();
            fullstats[iter->first] = stat;
        }
        root[CBSDKD_MSGFLD_DSRES_FULL] = fullstats;
    }
}

} /* namespace CBSdkd */
