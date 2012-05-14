/*
 * Handle.h
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#ifndef HANDLE_H_
#define HANDLE_H_
#include <sys/types.h>
#include <libcouchbase/couchbase.h>

#include "Request.h"
#include "Response.h"
#include "Error.h"
#include "Dataset.h"
#include "Message.h"
#include "utils.h"
#include "cbsdkd.h"

namespace CBSdkd {
using std::map;


class Handle;
class ResultOptions;

class MutateParams {
public:
    std::string oper;
    unsigned int expiry;
};

class ResultOptions {

public:
    bool full;
    bool multi;
    unsigned int expiry;
    unsigned int delay;

    ResultOptions(const Json::Value& opts)
    :
        full(opts[CBSDKD_MSGFLD_DSREQ_FULL].asBool()),
        multi(opts[CBSDKD_MSGFLD_DSREQ_MULTI].asBool()),
        expiry(opts[CBSDKD_MSGFLD_DSREQ_EXPIRY].asUInt()),
        delay(opts[CBSDKD_MSGFLD_DSREQ_DELAY].asUInt())
    {
    }

    ResultOptions(bool full = false,
                  unsigned int expiry = 0,
                  unsigned int delay = 0) :
        full(full), expiry(expiry), delay(delay) { }

};

// Result set/cookie

class FullResult {

public:

    FullResult(std::string s) : str(s), status(0) { }
    FullResult(int err) : str(""), status(err) { }
    FullResult() : str(""), status(Error::SUBSYSf_MEMD|Error::MEMD_ENOENT) { }

    operator int () const { return status; }
    operator std::string () const { return str; }
    operator bool () const {
        return this->status == 0;
    }

    int getStatus() const  { return status; }
    std::string getString() const { return str; }

private:
    std::string str;
    int status;
};

class ResultSet {
public:
    ResultSet() :
        remaining(0),
        parent(NULL) {}

    void setError(libcouchbase_t, libcouchbase_error_t,
                  const std::string& key);


    std::map<int,int> stats;
    std::map<std::string,FullResult> fullstats;

    ResultOptions options;

    Error getError() {
        return oper_error;
    }

    void resultsJson(Json::Value *in) const;

    void clear() {
        stats.clear();
        fullstats.clear();
        remaining = 0;
        parent = NULL;
    }

    Error oper_error;
    unsigned int remaining;

private:
    const Handle* parent;
};


class HandleOptions {
public:
    HandleOptions() { }
    virtual ~HandleOptions() { }

    HandleOptions(const Json::Value& json) :
        hostname(json[CBSDKD_MSGFLD_HANDLE_HOSTNAME].asString()),
        username(json[CBSDKD_MSGFLD_HANDLE_USERNAME].asString()),
        password(json[CBSDKD_MSGFLD_HANDLE_PASSWORD].asString()),
        bucket(json[CBSDKD_MSGFLD_HANDLE_BUCKET].asString())
    {
        const Json::Value& opts = json[CBSDKD_MSGFLD_HANDLE_OPTIONS];
        if ( opts.asBool() ) {
            timeout = opts[CBSDKD_MSGFLD_HANDLE_OPT_TMO].asUInt();
        } else {
            timeout = 0;
        }
    }

    bool isValid() {
        return (hostname.size() && bucket.size());
    }

    std::string hostname,
                username,
                password,
                bucket;

    unsigned long timeout;
};

class Handle {
public:
    virtual ~Handle();

    Handle(const HandleOptions& options);

    bool connect(Error *errp);

    bool
    dsGet(Command cmd,
          Dataset const& ds, ResultSet& out,
          ResultOptions const& options = ResultOptions());

    bool
    dsKeyop(Command cmd,
            Dataset const& ds, ResultSet& out,
            ResultOptions const& options = ResultOptions());

    bool
    dsMutate(Command cmd,
             Dataset const&, ResultSet& out,
             ResultOptions const& options = ResultOptions());

    static int
    mapError(libcouchbase_error_t err, int defl = Error::SUCCESS) {
        if (err == LIBCOUCHBASE_SUCCESS) {
            return 0;
        }
        if (Errmap.find(err) != Errmap.end()) {
            return Errmap[err];
        }
        return defl;
    }


private:
    HandleOptions options;
    bool is_connected;

    int ifd;
    int ofd;

    std::vector<ResultSet>pending_results;
    std::vector<Error>pending_errors;

    libcouchbase_t instance;

    static std::map<libcouchbase_error_t,int> Errmap;

    // Callbacks for libcouchbase
    static void
    cb_err(libcouchbase_t, libcouchbase_error_t, const char*);

    static void
    cb_get(libcouchbase_t, ResultSet*,
           libcouchbase_error_t,
           const void *, libcouchbase_size_t,
           const void *, libcouchbase_size_t,
           libcouchbase_uint32_t, libcouchbase_cas_t);

    static void
    cb_storage(libcouchbase_t, ResultSet*,
               libcouchbase_storage_t,libcouchbase_error_t,
               const void*, libcouchbase_size_t,
               libcouchbase_cas_t);

    static void
    cb_keyop(libcouchbase_t, ResultSet*,
             libcouchbase_error_t,
             const void*, libcouchbase_size_t);
};

#define CBSDKD_XERRMAP(X) \
X(LIBCOUCHBASE_BUCKET_ENOENT,    Error::SUBSYSf_CLUSTER|Error::MEMD_ENOENT) \
X(LIBCOUCHBASE_AUTH_ERROR,       Error::SUBSYSf_CLUSTER|Error::CLUSTER_EAUTH) \
X(LIBCOUCHBASE_CONNECT_ERROR,    Error::SUBSYSf_NETWORK|Error::ERROR_GENERIC) \
X(LIBCOUCHBASE_NETWORK_ERROR,    Error::SUBSYSf_NETWORK|Error::ERROR_GENERIC) \
X(LIBCOUCHBASE_ENOMEM,           Error::SUBSYSf_MEMD|Error::ERROR_GENERIC) \
X(LIBCOUCHBASE_KEY_ENOENT,       Error::SUBSYSf_MEMD|Error::MEMD_ENOENT) \
X(LIBCOUCHBASE_ETIMEDOUT,        Error::SUBSYSf_CLIENT|Error::CLIENT_ETMO) \
X(LIBCOUCHBASE_ETMPFAIL,         Error::SUBSYSf_CLIENT| \
                                Error::SUBSYSf_NETWORK| \
                                                        Error::ERROR_GENERIC);



} /* namespace CBSdkd */
#endif /* HANDLE_H_ */
