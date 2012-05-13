/*
 * Handle.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include "/home/mnunberg/src/cbsdkd/cplusplus/Handle.h"
#include <cstdlib>
#include <cstdio>


namespace CBSdkd {

std::map<libcouchbase_error_t,int> Handle::Errmap =
        create_map<libcouchbase_error_t,int>
#define X(a,b) (a,b)
CBSDKD_XERRMAP(X);
#undef X

Handle::Handle(Request const& r) :
        is_connected(false),
        req(r),
        instance(NULL)
{
}

Handle::~Handle() {
    if (instance != NULL) {
        libcouchbase_destroy(instance);
    }
    instance = NULL;
}

bool
Handle::verifyRequest(const Request& req, std::string *errmsg)
{
    std::string myerr = "";
    const Json::Value& params = req.payload;
    if ( (!params["Hostname"]) || (!params["Bucket"])) {
        myerr = "Missing hostname or bucket";
    }
    if (myerr.size()) {
        errmsg->assign(myerr);
        return false;
    }
    errmsg->assign("");
    return true;
}

bool
Handle::connect(Error *errp)
{
    // Gather parameters
    libcouchbase_error_t the_error;
    Json::Value const& params = this->req.payload;
    instance = libcouchbase_create(params["Hostname"].asCString(),
                                   params["Username"].asCString(),
                                   params["Password"].asCString(),
                                   params["Bucket"].asCString(),
                                   NULL);
    if (!instance) {
        errp->setCode(Error::SUBSYSf_CLIENT|Error::ERROR_GENERIC);
        errp->errstr = "Could not construct handle";
        return false;
    }
    if (params["Options"].isObject() &&
            params["Options"]["Timeout"].isIntegral()) {
        libcouchbase_set_timeout(instance,
                                 (unsigned int)
                                 (params["Options"]["Timeout"].asFloat() *
                                 1000000));
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
    if (err != LIBCOUCHBASE_SUCCESS) {
        printf("Got error (%d)\n", err);
    }
    rs->stats[myerr]++;
    rs->remaining--;
    if (rs->options.full) {
        std::string str;
        str.assign((const char*)key, nkey);
        rs->fullstats[str] = myerr;
    }
}

void
Handle::cb_keyop(libcouchbase_t instance, ResultSet * rs,
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

ResultSet
Handle::dsGet(Command cmd, Dataset const &ds, const Json::Value& options)
{
    ResultSet ret;
    ret.options = ResultOptions(options);

    libcouchbase_time_t exp = ret.options.expiry;
    DatasetIterator* iter = ds.getIter();

    for (iter->start(); iter->done() == false; iter->advance()) {

        std::string k = iter->key();
        libcouchbase_size_t sz = k.size();
        const char *cstr = k.c_str();

        libcouchbase_error_t err =
                libcouchbase_mget(instance, &ret, 1,
                                  (const void**)&cstr, &sz, &exp);
        if (err == LIBCOUCHBASE_SUCCESS) {
            ret.remaining++;
        } else {
            ret.setError(instance, err, k);
        }
    }

    delete iter;

    if (ret.remaining) {
        libcouchbase_wait(instance);
    }

    return ret;
}

ResultSet
Handle::dsMutate(Command cmd,
                 const Dataset& ds, const Json::Value& options)
{
    ResultSet ret;
    ret.options = ResultOptions(options);
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
        ret.oper_error = Error(Error::SUBSYSf_SDKD,
                               Error::SDKD_EINVAL,
                               "Unknown mutation operation");
        return ret;
    }

    libcouchbase_time_t exp = ret.options.expiry;
    DatasetIterator *iter = ds.getIter();


    for (iter->start(); iter->done() == false; iter->advance()) {
        std::string k = iter->key(), v = iter->value();
        libcouchbase_size_t ksz = k.size(), vsz = v.size();
        const char *kstr = k.data(), *vstr = v.data();

        libcouchbase_error_t err =
                libcouchbase_store(instance, &ret, storop,
                                   kstr, ksz,
                                   vstr, vsz,
                                   0, exp, 0);

        if (err == LIBCOUCHBASE_SUCCESS) {
            ret.remaining++;
        } else {
            ret.setError(instance, err, k);
        }
        if (ret.options.delay) {
            usleep(ret.options.delay * 1000);
        }
    }

    if (ret.remaining) {
        libcouchbase_wait(instance);
    }
    return ret;
}

ResultSet
Handle::dsKeyop(Command cmd,
                const Dataset& ds, const Json::Value& options)
{
    ResultSet ret;
    ret.options = ResultOptions(options);
    libcouchbase_time_t exp = ret.options.expiry;

    DatasetIterator *iter = ds.getIter();

    for (iter->start(); iter->done() == false; iter->advance()) {
        std::string k = iter->key();
        const char *kstr = k.data();
        libcouchbase_size_t ksz = k.size();
        libcouchbase_error_t err;
        if (cmd == Command::MC_DS_DELETE) {
            err = libcouchbase_remove(instance, &ret, kstr, ksz, 0);
        } else {
            err = libcouchbase_mtouch(instance, &ret,
                                      1, (const void**)&kstr, &ksz,
                                      &exp);
        }
        if (err == LIBCOUCHBASE_SUCCESS) {
            ret.remaining++;
        } else {
            ret.setError(instance, err, k);
        }
        if (ret.options.delay) {
            usleep(ret.options.delay * 1000);
        }
    }
    if (ret.remaining) {
        libcouchbase_wait(instance);
    }
    return ret;
}

} /* namespace CBSdkd */
