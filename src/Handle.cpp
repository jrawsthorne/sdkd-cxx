/*
 * Handle.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include "sdkd_internal.h"

namespace CBSdkd {


extern "C" {

void Handle::VersionInfoJson(Json::Value &res) {
    Json::Value caps;
    Json::Value config;
    Json::Value rtComponents;
    Json::Value hdrComponents;
    char vbuf[1000] = { 0 };

    const DaemonOptions& dOpts =
            Daemon::MainDaemon
                ? Daemon::MainDaemon->getOptions()
                : DaemonOptions();

    lcb_uint32_t vout = 0;
    lcb_get_version(&vout);
    sprintf(vbuf, "0x%X", vout);
    rtComponents["SDK"] = vbuf;

    sprintf(vbuf, "0x%x", LCB_VERSION);
    hdrComponents["SDK"] = vbuf;

// Thanks mauke
#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)
    hdrComponents["CHANGESET"] = STRINGIFY(LCB_VERSION_CHANGESET);
#undef STRINGIFY
#undef STRINGIFY_

    config["IO_PLUGIN"] = dOpts.ioPluginName ? dOpts.ioPluginName : "";
    config["CONNCACHE"] = dOpts.conncachePath ? dOpts.conncachePath : "";
    config["RECONNECT"] = dOpts.noPersist;

    caps["CANCEL"] = true;
    caps["DS_SHARED"] = true;
    caps["CONTINUOUS"] = true;
    caps["PREAMBLE"] = false;
    caps["VIEWS"] = true;

    res["CAPS"] = caps;
    res["RUNTIME"] = rtComponents;
    res["HEADERS"] = hdrComponents;
    res["CONFIG"] = config;
    res["TIME"] = (Json::UInt64)time(NULL);
}



static void cb_err(lcb_t instance, lcb_error_t err, const char *desc)
{
    Handle *handle = (Handle*)lcb_get_cookie(instance);
    int myerr = Handle::mapError(err);
    handle->appendError(myerr, desc ? desc : "");
    log_noctx_error("Got error %d: %s", err, desc ? desc : "");
}

static void cb_config(lcb_t instance, lcb_configuration_t config)
{
    (void)instance;
    (void)config;
    // Too verbose
    //log_noctx_trace("Instance %p: CONFIG UPDATE [%d]", instance, config);
}

static void cb_remove(lcb_t instance, int, const lcb_RESPBASE *resp)
{
    reinterpret_cast<ResultSet*>(resp->cookie)->setRescode(resp->rc,
            resp->key, resp->nkey);
}

static void cb_touch(lcb_t instance, int, const lcb_RESPBASE *resp)
{
    reinterpret_cast<ResultSet*>(resp->cookie)->setRescode(resp->rc,
            resp->key, resp->nkey);
}

static void cb_storage(lcb_t instance, int, const lcb_RESPBASE *resp)
{
    reinterpret_cast<ResultSet*>(resp->cookie)->setRescode(resp->rc,
            resp->key, resp->nkey);
}

static void cb_get(lcb_t instance, int, const lcb_RESPBASE *resp)
{
    lcb_RESPGET* gresp = (lcb_RESPGET *)resp;
    reinterpret_cast<ResultSet*>(gresp->cookie)->setRescode(gresp->rc,
            gresp->key, gresp->nkey, true,
            gresp->value, gresp->nvalue);
}

static void cb_endure(lcb_t instance, int, const lcb_RESPBASE *resp)
{
    reinterpret_cast<ResultSet*>(resp->cookie)->setRescode(resp->rc,
            resp->key, resp->nkey);
}

static void cb_observe(lcb_t instance, int, const lcb_RESPBASE *resp)
{
    lcb_RESPOBSERVE *obresp = (lcb_RESPOBSERVE *)resp;
    ResultSet *out = reinterpret_cast<ResultSet*>(resp->cookie);

    if (obresp->rc == LCB_SUCCESS) {
        if (obresp->rflags & LCB_RESP_F_FINAL) {
            if (out->options.persist != out->obs_persist_count) {
                fprintf(stderr, "Item persistence not matched Received %d Expected %d",
                        out->obs_persist_count, out->options.persist);
            }
            if (out->options.replicate != out->obs_replica_count) {
                fprintf(stderr, "Item replication not matched Received %d Expected %d",
                        out->obs_replica_count, out->options.replicate);
            }
            out->setRescode(obresp->rc, obresp->key, obresp->nkey);
        }
        if (obresp->status == 1) {
            out->obs_persist_count++;
            out->obs_replica_count++;
        }
        if (obresp->status == 0)  {
            out->obs_replica_count++;
        }
    } else {
        out->setRescode(obresp->rc, obresp->key, obresp->nkey);
    }
}

static void cb_stats(lcb_t instance, int, const lcb_RESPBASE *resp)
{
    lcb_RESPSTATS *sresp = (lcb_RESPSTATS *)resp;
    ResultSet *out = reinterpret_cast<ResultSet*>(sresp->cookie);

    if (sresp->rc == LCB_SUCCESS) {
        if (strncmp((const char *)sresp->key, "key_exptime", sresp->nkey) && sresp->nvalue > 0) {
            char buf[sresp->nvalue];
            memcpy(buf, sresp->value, sresp->nvalue);
            unsigned int exp_expiry = out->options.expiry;
            unsigned int expiry = *(unsigned int *)buf;
            if(exp_expiry  != expiry) {
                fprintf(stderr, "TTL not matched Received %d Expected %d\n", expiry, exp_expiry);
            }
        }
        if (strncmp((const char *)sresp->key, "key_flags", sresp->nkey) && sresp->nvalue > 0) {
            char buf[sresp->nvalue];
            memcpy(buf, sresp->value, sresp->nvalue);
            unsigned int exp_flags = out->options.flags;
            unsigned int flags = *(unsigned int *)buf;

            if(exp_flags != flags) {
                fprintf(stderr, "Flags not matched Received %d Expected %d\n", flags, exp_flags);
            }
        }
        if (sresp->rflags & LCB_RESP_F_FINAL) {
            out->setRescode(sresp->rc, sresp->key, sresp->nkey);
        }
    } else {
        out->setRescode(sresp->rc, sresp->key, sresp->nkey);
    }
}

static void wire_callbacks(lcb_t instance)
{
#define _setcb(t,cb) \
    lcb_install_callback3(instance, t, cb)
    _setcb(LCB_CALLBACK_STORE, cb_storage);
    _setcb(LCB_CALLBACK_GET, cb_get);
    _setcb(LCB_CALLBACK_REMOVE, cb_remove);
    _setcb(LCB_CALLBACK_TOUCH, cb_touch);
    _setcb(LCB_CALLBACK_ENDURE, cb_endure);
    _setcb(LCB_CALLBACK_OBSERVE, cb_observe);
    _setcb(LCB_CALLBACK_STATS, cb_stats);
#undef _setcb
}

} /* extern "C" */


Handle::Handle(const HandleOptions& opts) :
        options(opts),
        is_connected(false),
        instance(NULL)
{
    create_opts.version = 0;
}

Handle::~Handle() {
    if (instance != NULL) {
        lcb_destroy(instance);
    }

    if (io != NULL) {
        lcb_destroy_io_ops(io);
        io = NULL;
    }
    instance = NULL;
}

#define cstr_ornull(s) \
    ((s.size()) ? s.c_str() : NULL)



bool
Handle::connect(Error *errp)
{
    // Gather parameters
    lcb_error_t the_error;
    instance = NULL;

    if (!create_opts.v.v0.host) {
        create_opts.v.v0.host = cstr_ornull(options.hostname);
        create_opts.v.v0.user = cstr_ornull(options.username);
        create_opts.v.v0.passwd = cstr_ornull(options.password);
        create_opts.v.v0.bucket = cstr_ornull(options.bucket);
        if (Daemon::MainDaemon->getOptions().conncachePath) {
            memset(&cached_opts, 0, sizeof(cached_opts));
            memcpy(&cached_opts.createopt, &create_opts, sizeof(create_opts));
            cached_opts.cachefile = Daemon::MainDaemon->getOptions().conncachePath;
        }
    }

    io = Daemon::MainDaemon->createIO();
    create_opts.v.v0.io = io;

    if (Daemon::MainDaemon->getOptions().conncachePath) {
        the_error = lcb_create_compat(LCB_CACHED_CONFIG, &cached_opts, &instance, NULL);
    } else {
        the_error = lcb_create(&instance, &create_opts);
    }

    if (!instance) {
        errp->setCode(Error::SUBSYSf_CLIENT|Error::ERROR_GENERIC);
        errp->errstr = "Could not construct handle";
        return false;
    }

    if (options.timeout) {
        lcb_U32 timeout = options.timeout * 1000000;
        lcb_cntl(instance, LCB_CNTL_SET, LCB_CNTL_OP_TIMEOUT, &timeout);
    }

    lcb_set_error_callback(instance, cb_err);
    lcb_set_configuration_callback(instance, cb_config);
    lcb_set_cookie(instance, this);
    wire_callbacks(instance);

    the_error = lcb_connect(instance);
    if (the_error != LCB_SUCCESS) {
        errp->setCode(mapError(the_error));
        errp->errstr = lcb_strerror(instance, the_error);

        log_error("lcb_connect failed: %s", errp->prettyPrint().c_str());
        return false;
    }
    lcb_wait(instance);

    the_error = lcb_get_bootstrap_status(instance);
    if (the_error != LCB_SUCCESS) {
        errp->setCode(mapError(the_error));
        errp->errstr = lcb_strerror(instance, the_error);

        log_error("lcb_bootstrap status failed: %s 0x%X", errp->prettyPrint().c_str(), the_error);
        return false;
    }

    lcb_wait3(instance, LCB_WAIT_NOCHECK);

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
    lcb_wait3(instance, LCB_WAIT_DEFAULT);
}

void
Handle::postsubmit(ResultSet& rs, unsigned int nsubmit)
{

    rs.remaining += nsubmit;

    if (!rs.options.iterwait) {
        // everything is buffered up
        return;
    }

    if (rs.remaining < rs.options.iterwait) {
        return;
    }

    lcb_wait(instance);

    unsigned int wait_msec = rs.options.getDelay();

    if (wait_msec) {
        sdkd_millisleep(wait_msec);
    }

    if (Daemon::MainDaemon->getOptions().noPersist) {
        lcb_destroy(instance);
        Error e;
        connect(&e);
    }
}

bool
Handle::dsGet(Command cmd, Dataset const &ds, ResultSet& out,
              const ResultOptions& options)
{
    out.options = options;
    out.clear();
    do_cancel = false;

    lcb_time_t exp = out.options.expiry;

    DatasetIterator* iter = ds.getIter();
    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();
        log_trace("GET: %s", k.c_str());

        lcb_sched_enter(instance);
        lcb_CMDGET cmd = { 0 };
        LCB_CMD_SET_KEY(&cmd, k.data(), k.size());
        cmd.exptime = exp;

        out.markBegin();
        lcb_error_t err = lcb_get3(instance, &out, &cmd);
        lcb_sched_leave(instance);

        if (err == LCB_SUCCESS) {
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
    lcb_storage_t storop;
    do_cancel = false;

    if (cmd == Command::MC_DS_MUTATE_ADD) {
        storop = LCB_ADD;
    } else if (cmd == Command::MC_DS_MUTATE_SET) {
        storop = LCB_SET;
    } else if (cmd == Command::MC_DS_MUTATE_APPEND) {
        storop = LCB_APPEND;
    } else if (cmd == Command::MC_DS_MUTATE_PREPEND) {
        storop = LCB_PREPEND;
    } else if (cmd == Command::MC_DS_MUTATE_REPLACE) {
        storop = LCB_REPLACE;
    } else {
        out.oper_error = Error(Error::SUBSYSf_SDKD,
                               Error::SDKD_EINVAL,
                               "Unknown mutation operation");
        return false;
    }

    lcb_time_t exp = out.options.expiry;
    DatasetIterator *iter = ds.getIter();

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key(), v = iter->value();

        lcb_sched_enter(instance);
        lcb_CMDSTORE cmd = { 0 };
        cmd.operation = storop;

        LCB_CMD_SET_KEY(&cmd, k.data(), k.size());
        LCB_CMD_SET_VALUE(&cmd, v.data(), v.size());
        cmd.exptime = exp;
        cmd.flags = out.options.flags;


        out.markBegin();
        lcb_error_t err = lcb_store3(instance, &out, &cmd);
        lcb_sched_leave(instance);

        if (err == LCB_SUCCESS) {
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
Handle::dsGetReplica(Command cmd, Dataset const &ds, ResultSet& out,
              const ResultOptions& options)
{
    out.options = options;
    out.clear();
    do_cancel = false;

    DatasetIterator* iter = ds.getIter();

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();
        log_trace("GET REPLICA : %s", k.c_str());

        lcb_sched_enter(instance);
        lcb_CMDGETREPLICA cmd = { 0 };
        LCB_CMD_SET_KEY(&cmd, k.data(), k.size());

        out.markBegin();
        lcb_error_t err = lcb_rget3(instance, &out, &cmd);
        lcb_sched_leave(instance);

        if (err == LCB_SUCCESS) {
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
Handle::dsEndure(Command cmd, Dataset const &ds, ResultSet& out,
        const ResultOptions& options)
{
    out.options = options;
    out.clear();
    do_cancel = false;

    DatasetIterator* iter = ds.getIter();

    //Use the same context for all endure commands
    lcb_durability_opts_t dopts = { 0 };
    dopts.v.v0.persist_to = options.persist;
    dopts.v.v0.replicate_to = options.replicate;
    dopts.v.v0.cap_max = 1;

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key(), v = iter->value();

        lcb_CMDENDURE cmd = { 0 };
        LCB_CMD_SET_KEY(&cmd, k.data(), k.size());

        out.markBegin();

        lcb_MULTICMD_CTX *mctx = lcb_endure3_ctxnew(instance, &dopts, NULL);
        lcb_sched_enter(instance);
        mctx->addcmd(mctx, (lcb_CMDBASE*)&cmd);
        lcb_error_t err =  mctx->done(mctx, &out);
        lcb_sched_leave(instance);

        if (err == LCB_SUCCESS) {
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
Handle::dsObserve(Command cmd, Dataset const &ds, ResultSet& out,
              const ResultOptions& options)
{
    out.options = options;
    out.clear();
    do_cancel = false;

    DatasetIterator* iter = ds.getIter();

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();

        lcb_CMDOBSERVE cmd = {0};
        LCB_CMD_SET_KEY(&cmd, k.data(), k.size());

        out.markBegin();

        lcb_MULTICMD_CTX *mctx = lcb_observe3_ctxnew(instance);
        lcb_sched_enter(instance);
        mctx->addcmd(mctx, (lcb_CMDBASE*)&cmd);
        lcb_error_t err =  mctx->done(mctx, &out);
        lcb_sched_leave(instance);

        if (err == LCB_SUCCESS) {
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
Handle::dsEndureWithSeqNo(Command cmd, Dataset const &ds, ResultSet& out,
              const ResultOptions& options)
{
    out.options = options;
    out.clear();
    do_cancel = false;

    DatasetIterator* iter = ds.getIter();

    //Use the same context for all endure commands
    lcb_durability_opts_t dopts = { 0 };
    dopts.v.v0.persist_to = options.persist;
    dopts.v.v0.replicate_to = options.replicate;
    dopts.v.v0.cap_max = 1;
    dopts.v.v0.pollopts = LCB_DURABILITY_METH_SEQNO;

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key(), v = iter->value();

        lcb_CMDENDURE cmd = { 0 };
        LCB_CMD_SET_KEY(&cmd, k.data(), k.size());

        out.markBegin();

        lcb_MULTICMD_CTX *mctx = lcb_endure3_ctxnew(instance, &dopts, NULL);
        lcb_sched_enter(instance);
        mctx->addcmd(mctx, (lcb_CMDBASE*)&cmd);
        lcb_error_t err =  mctx->done(mctx, &out);
        lcb_sched_leave(instance);

        if (err == LCB_SUCCESS) {
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
Handle::dsKeyop(Command cmd, const Dataset& ds, ResultSet& out,
                const ResultOptions& options)
{
    out.options = options;
    out.clear();
    DatasetIterator *iter = ds.getIter();
    do_cancel = false;

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();
        lcb_error_t err;

        out.markBegin();

        if (cmd == Command::MC_DS_DELETE) {
            lcb_sched_enter(instance);
            lcb_CMDREMOVE cmd = { 0 };
            LCB_CMD_SET_KEY(&cmd, k.data(), k.size());
            err = lcb_remove3(instance, &out, &cmd);
            lcb_sched_leave(instance);
        } else {
            lcb_sched_enter(instance);
            lcb_CMDTOUCH cmd = { 0 };
            LCB_CMD_SET_KEY(&cmd, k.data(), k.size());
            err = lcb_touch3(instance, &out, &cmd);
            lcb_sched_leave(instance);
        }

        if (err == LCB_SUCCESS) {
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
Handle::dsVerifyStats(Command cmd, const Dataset& ds, ResultSet& out,
        const ResultOptions& options) {

    out.options = options;
    out.clear();
    DatasetIterator *iter = ds.getIter();
    do_cancel = false;

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();

        lcb_sched_enter(instance);
        lcb_CMDSTATS cmd = { 0 };
        memset(&cmd, 0, sizeof(cmd));

        LCB_KREQ_SIMPLE(&cmd.key, k.data(), k.size());
        cmd.cmdflags = LCB_CMDSTATS_F_KV;

        out.markBegin();

        lcb_error_t err =  lcb_stats3(instance, &out, &cmd);
        lcb_sched_leave(instance);

        if (err == LCB_SUCCESS) {
            postsubmit(out);
        } else {
            out.setRescode(err, k, true);
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
