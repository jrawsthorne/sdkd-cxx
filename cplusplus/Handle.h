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
#include <pthread.h>
#include "Message.h"

namespace CBSdkd {
using std::map;

template <typename T, typename U>
// stolen from http://stackoverflow.com/a/1730798/479941
class create_map
{
private:
    std::map<T, U> m_map;
public:
    create_map(const T& key, const U& val) { m_map[key] = val; }
    create_map<T, U>& operator()(const T& key, const U& val)
    { m_map[key] = val;  return *this; }
    operator std::map<T, U>()
    { return m_map; }
};

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
        full(opts["Full"].asBool()),
        multi(opts["Multi"].asBool()),
        expiry(opts["Expiry"].asUInt()),
        delay(opts["Delay"].asUInt())
    {
    }

    ResultOptions(bool full = false,
                  unsigned int expiry = 0,
                  unsigned int delay = 0) :
        full(full), expiry(expiry), delay(delay) { }

};

// Result set/cookie
class ResultSet {
public:
    ResultSet() :
        remaining(0),
        parent(NULL) {}

    void setError(libcouchbase_t, libcouchbase_error_t,
                  const std::string& key);



    std::map<int,int> stats;
    std::map<std::string,std::string> fullstats;
    ResultOptions options;

    Error oper_error;
    unsigned int remaining;
    const Handle* parent;
};


class Handle {
public:
    virtual ~Handle();

    Handle(Request const&);

    static bool verifyRequest(Request const&, std::string *errmsg);

    bool connect(Error *errp);

    ResultSet
    dsGet(Command cmd,
          Dataset const& ds, Json::Value const& options);



    ResultSet
    dsKeyop(Command cmd,
            Dataset const& ds, Json::Value const& options);

    ResultSet
    dsMutate(Command cmd,
             Dataset const&, Json::Value const& options);

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
    bool is_connected;
    Request const req;

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
