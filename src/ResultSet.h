#ifndef SDKD_RESULTSET_H_
#define SDKD_RESULTSET_H_

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#define CBSDKD_XERRMAP(X) \
X(LCB_BUCKET_ENOENT,    Error::SUBSYSf_CLUSTER|Error::MEMD_ENOENT) \
X(LCB_AUTH_ERROR,       Error::SUBSYSf_CLUSTER|Error::CLUSTER_EAUTH) \
X(LCB_CONNECT_ERROR,    Error::SUBSYSf_NETWORK|Error::ERROR_GENERIC) \
X(LCB_NETWORK_ERROR,    Error::SUBSYSf_NETWORK|Error::ERROR_GENERIC) \
X(LCB_ENOMEM,           Error::SUBSYSf_MEMD|Error::ERROR_GENERIC) \
X(LCB_KEY_ENOENT,       Error::SUBSYSf_MEMD|Error::MEMD_ENOENT) \
X(LCB_ETIMEDOUT,        Error::SUBSYSf_CLIENT|Error::CLIENT_ETMO)



namespace CBSdkd {
class Handle;

class ResultOptions {
public:
    bool full;
    unsigned int multi;
    unsigned int expiry;
    unsigned int flags;
    unsigned int iterwait;

    unsigned int delay_min;
    unsigned int delay_max;
    unsigned int delay;

    unsigned int timeres;
    unsigned int persist;
    unsigned int replicate;

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
        time_min(-1),
        time_max(0),
        time_avg(0),
        count(0)
    { }

    virtual ~TimeWindow() { }

    // Duration statistics
    unsigned int time_total;
    unsigned int time_min;
    unsigned int time_max;
    unsigned int time_avg;

    unsigned count;
    std::map<int, int> ec;
};

class ResultSet {
public:
    ResultSet() :
        remaining(0),
        vresp_complete(false),
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
    void setRescode(lcb_error_t err, const void* key, size_t nkey,
                    bool expect_value, const void* value, size_t n_value);

    void setRescode(lcb_error_t err) {
        setRescode(err, NULL, 0, false, NULL, 0);
    }

    void setRescode(lcb_error_t err,
                    const void *key, size_t nkey) {
        setRescode(err, key, nkey, false, NULL, 0);
    }

    void setRescode(lcb_error_t err, const std::string key,
                    bool expect_value) {
        setRescode(err, key.c_str(), key.length(), true, NULL, 0);
    }

    void setRescode(lcb_error_t err, bool isFinal) {
        vresp_complete = isFinal;
        setRescode(err, NULL, 0, false, NULL, 0);
    }

    std::map<int,int> stats;
    std::map<std::string,FullResult> fullstats;
    std::vector<TimeWindow> timestats;

    ResultOptions options;

    Error getError() {
        return oper_error;
    }

    Error setError(Error newerr) {
        Error old = oper_error;
        oper_error = newerr;
        return old;
    }

    void resultsJson(Json::Value *in) const;

    void markBegin() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        opstart_tmsec = getEpochMsecs();
    }

    static suseconds_t getEpochMsecs(timeval& tv) {
        gettimeofday(&tv, NULL);
        return (suseconds_t)(((double)tv.tv_usec / 1000.0) + tv.tv_sec * 1000);
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

    static int
    mapError(lcb_error_t err) {
        if (err == LCB_SUCCESS) {
            return 0;
        }
        if (Errmap.find(err) != Errmap.end()) {
            return Errmap[err];
        }

        int ret = Error::SUBSYSf_SDK;
        ret |= err << 8;
        return ret;
    }

    static std::map<lcb_error_t,int> Errmap;
    unsigned int obs_persist_count;
    unsigned int obs_replica_count;
    unsigned int query_doc_insert_count;
    unsigned int query_resp_count;
    bool vresp_complete;

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


} // namespace
#endif /* SDKD_RESULTSET_H_ */
