/*
 * Handle.h
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#ifndef HANDLE_H_
#define HANDLE_H_

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

namespace CBSdkd {
using std::map;


class Handle;
class ResultOptions;

class MutateParams {
public:
    std::string oper;
    unsigned int expiry;
};


class Handle;



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

    void appendError(int err, const char *desc) {
        pending_errors.push_back(Error(err, desc));
    }

    libcouchbase_t getLcb() {
        return instance;
    }

    // Helper Functions for cancellation
    bool isCancelled() {
        return do_cancel;
    }
    void externalEnter() {
        do_cancel = false;
    }
    void externalLeave() {
        /*no-op*/
    }

    // Cancels the current operation, causing it to return during the next
    // operation.
    void cancelCurrent();

    static int
    mapError(libcouchbase_error_t err, int defl = Error::SUCCESS) {
        return ResultSet::mapError(err, defl);
    }

private:
    HandleOptions options;
    bool is_connected;
    bool do_cancel;

    int ifd;
    int ofd;

    std::vector<ResultSet>pending_results;
    std::vector<Error>pending_errors;

    libcouchbase_t instance;

    void collect_result(ResultSet& rs);
    void postsubmit(ResultSet& rs, unsigned int nsubmit = 1);
};


} /* namespace CBSdkd */
#endif /* HANDLE_H_ */
