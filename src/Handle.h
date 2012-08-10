/*
 * Handle.h
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#ifndef HANDLE_H_
#define HANDLE_H_
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include <libcouchbase/couchbase.h>

#include "Request.h"
#include "Response.h"
#include "Error.h"
#include "Dataset.h"
#include "Message.h"
#include "utils.h"
#include "cbsdkd.h"
#include "contrib/debug++.h"

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
    unsigned int multi;
    unsigned int expiry;
    bool iterwait;

    unsigned int delay_min;
    unsigned int delay_max;
    unsigned int delay;

    unsigned int timeres;

    ResultOptions(const Json::Value&);
    ResultOptions(bool full = false,
                  unsigned int expiry = 0,
                  unsigned int delay = 0);

    unsigned int getDelay() const;

private:
    void _determine_delay();
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

class TimeWindow {
public:
    TimeWindow() :
        time_total(0),
        count(0),
        time_min(-1),
        time_max(0),
        time_avg(0)
    { }

    virtual ~TimeWindow() { }

    unsigned time_total, count, time_min, time_max, time_avg;
    std::map<int, int> ec;
};

class Handle;

class ResultSet {
public:
    ResultSet() :
        remaining(0),
        parent(NULL),
        dsiter(NULL)
    {
        clear();
    }

    // Sets the status for a key
    // @param err the error from the operation
    // @param key the key (mandatory)
    // @param nkey the size of the key (mandatory)
    // @param expect_value whether this operation should have returned a value
    // @param value the value (can be NULL)
    // @param n_value the size of the value
    void setRescode(libcouchbase_error_t err, const void* key, size_t nkey,
                    bool expect_value, const void* value, size_t n_value);


    void setRescode(libcouchbase_error_t err,
                    const void *key, size_t nkey) {
        setRescode(err, key, nkey, false, NULL, 0);
    }

    void setRescode(libcouchbase_error_t err, const std::string key,
                    bool expect_value) {
        setRescode(err, key.c_str(), key.length(), true, NULL, 0);
    }

    std::map<int,int> stats;
    std::map<std::string,FullResult> fullstats;
    std::vector<TimeWindow> timestats;

    ResultOptions options;

    Error getError() {
        return oper_error;
    }

    void resultsJson(Json::Value *in) const;

    void markBegin() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        opstart_tmsec = getEpochMsecs();
    }

    static suseconds_t getEpochMsecs(timeval& tv) {
        gettimeofday(&tv, NULL);
        return (tv.tv_usec / 1000.0) + tv.tv_sec * 1000;
    }

    static suseconds_t getEpochMsecs() {
        struct timeval tv;
        return getEpochMsecs(tv);
    }

    void clear() {

        stats.clear();
        fullstats.clear();
        timestats.clear();
        remaining = 0;
        parent = NULL;

        win_begin = 0;
        cur_wintime = 0;
        opstart_tmsec = 0;
    }

    Error oper_error;
    unsigned int remaining;

private:
    friend class Handle;
    const Handle* parent;
    DatasetIterator *dsiter;

    // Timing stuff
    time_t win_begin;
    time_t cur_wintime;

    // Time at which this result set was first batched
    suseconds_t opstart_tmsec;
};


class HandleOptions {
public:
    HandleOptions() { }
    virtual ~HandleOptions() { }

    HandleOptions(const Json::Value& json) :
        hostname(json[CBSDKD_MSGFLD_HANDLE_HOSTNAME].asString()),
        username(""),
        password(""),
        bucket(json[CBSDKD_MSGFLD_HANDLE_BUCKET].asString())
    {
        const Json::Value& opts = json[CBSDKD_MSGFLD_HANDLE_OPTIONS];
        if ( opts.asBool() ) {
            timeout = opts[CBSDKD_MSGFLD_HANDLE_OPT_TMO].asUInt();
            username = opts[CBSDKD_MSGFLD_HANDLE_USERNAME].asString();
            password = opts[CBSDKD_MSGFLD_HANDLE_PASSWORD].asString();
        } else {
            timeout = 0;
        }

        if (json[CBSDKD_MSGFLD_HANDLE_PORT].asInt()) {
            hostname += ":";
            stringstream ss;
            ss << json[CBSDKD_MSGFLD_HANDLE_PORT].asInt();
            hostname += ss.str();
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

class Handle : protected DebugContext {
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

    void appendError(int err, const char *desc) {
        pending_errors.push_back(Error(err, desc));
    }

    // Cancels the current operation, causing it to return during the next
    // operation.
    void cancelCurrent();

private:
    HandleOptions options;
    bool is_connected;
    bool do_cancel;

    int ifd;
    int ofd;

    std::vector<ResultSet>pending_results;
    std::vector<Error>pending_errors;

    libcouchbase_t instance;

    static std::map<libcouchbase_error_t,int> Errmap;
    void collect_result(ResultSet& rs);
    void postsubmit(ResultSet& rs, unsigned int nsubmit = 1);
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
