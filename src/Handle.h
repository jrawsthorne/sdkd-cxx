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
        if ( opts.isObject() ) {

            string host_extra = "";
            if (!getExtraHosts(opts, host_extra)) {
                hostname = "";
                return; // invalidate
            }

            if (host_extra.size()) {
                if (hostname.size()) {
                    hostname += ";";
                }
                hostname += host_extra;
            }

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

private:
    // Gets extra hosts, if available. Returns false on error
    bool getExtraHosts(Json::Value opts, string& extras) {
        const Json::Value jothers = opts[CBSDKD_MSGFLD_HANDLE_OPT_BACKUPS];
        if (jothers.isNull()) {
            return true; // doesn't exist
        }
        if (!jothers.isArray()) {
            log_noctx_error("Expected array. Got something else instead");
            return false;
        }

        for (Json::Value::iterator iter = jothers.begin();
                iter != jothers.end(); iter++) {
            Json::Value& curbu = *iter;

            if (curbu.isString()) {
                extras += curbu.asString();

            } else if (curbu.isArray()) {
                if (curbu.size() > 2) {
                    log_noctx_error("Expected array of <= 2 elements, or string");
                    return false;
                }
                if (! curbu[0].isString()) {
                    log_noctx_error("First element must be a string");
                    return false;
                }

                extras += curbu[0].asString();
                if (curbu[1].isInt()) {
                    extras += ":";
                    extras += curbu[1].asString();
                }
            }

            extras += ";";
        }
        return true;
    }
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

    lcb_t getLcb() {
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
    mapError(lcb_error_t err, int defl = Error::SUCCESS) {
        return ResultSet::mapError(err, defl);
    }

    static void VersionInfoJson(Json::Value& res)
    {
        Json::Value caps;
        Json::Value components;
        const char *vstr;
        lcb_uint32_t vout = 0;


        vstr = lcb_get_version(&vout);
        components["SDK"] = vstr;
        components["SDK_VID"] = vout;

        caps["CANCEL"] = true;
        caps["DS_SHARED"] = true;
        caps["CONTINUOUS"] = true;
        caps["PREAMBLE"] = false;
        caps["VIEWS"] = true;

        res["CAPS"] = caps;
        res["COMPONENTS"] = components;
    }

private:
    HandleOptions options;
    lcb_create_st create_opts;
    lcb_cached_config_st cached_opts;

    bool is_connected;
    bool do_cancel;

    int ifd;
    int ofd;

    std::vector<ResultSet>pending_results;
    std::vector<Error>pending_errors;

    lcb_t instance;

    void collect_result(ResultSet& rs);
    void postsubmit(ResultSet& rs, unsigned int nsubmit = 1);
};


} /* namespace CBSdkd */
#endif /* HANDLE_H_ */
