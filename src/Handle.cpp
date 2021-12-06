/*
 * Handle.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include <regex>
#include "sdkd_internal.h"

namespace CBSdkd {


extern "C" {

// TODO: Get this from CXX_CLIENT
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

    rtComponents["SDKVersion"] = "";
    hdrComponents["SDKVersion"] = "";

// Thanks mauke
#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)
    hdrComponents["CHANGESET"] = "";
    fprintf(stderr, " SDK version changeset %s\n", hdrComponents["CHANGESET"].asString().c_str());
#undef STRINGIFY
#undef STRINGIFY_

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
    res["SDK"] = "cxx";
    res["CHANGESET"] = hdrComponents["CHANGESET"];
}

// static void cb_config(lcb_INSTANCE *instance, lcb_STATUS err)
// {
//     (void)instance;
//     if (err != LCB_SUCCESS) {
//         log_noctx_error("Got error 0x%x", err);
//     }
// }



// static void cb_storedur(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp)
// {
//     const lcb_RESPSTORE *rb = (const lcb_RESPSTORE *)resp;
//     void *cookie;
//     const char* key;
//     size_t nkey;
//     lcb_respstore_key(rb, &key, &nkey);
//     lcb_respstore_cookie(rb, &cookie);
//     reinterpret_cast<ResultSet*>(cookie)->setRescode(lcb_respstore_status(rb),
//             key, nkey);
// }


// static void cb_endure(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp)
// {
//     lcb_RESPSTORE* dresp = (lcb_RESPSTORE *)resp;
//     void *cookie;
//     const char* key;
//     size_t nkey;
//     int persisted_master;
//     lcb_respstore_key(dresp, &key, &nkey);
//     lcb_respstore_cookie(dresp, &cookie);
//     lcb_respstore_observe_master_persisted(dresp, &persisted_master);
//     if (persisted_master == 0) {
//         reinterpret_cast<ResultSet*>(cookie)->setRescode(lcb_respstore_status(dresp), key, nkey);
//     } else {
//         reinterpret_cast<ResultSet*>(cookie)->setRescode(LCB_ERR_GENERIC, key, nkey);
//     }
// }

// static void cb_observe(lcb_INSTANCE *instance, int, const lcb_RESPBASE *resp)
// {
//     lcb_RESPSTORE *obresp = (lcb_RESPSTORE *)resp;
//     void *cookie;
//     int rflags;
//     const char* key;
//     size_t nkey;
//     uint64_t cas;
//     lcb_respstore_cookie(obresp, &cookie);
//     rflags = lcb_respstore_observe_attached(obresp);
//     lcb_respstore_key(obresp, &key, &nkey);
//     lcb_respstore_cas(obresp, &cas);
//     int ismaster;
//     lcb_respstore_observe_master_exists(obresp, &ismaster);
//     ResultSet *out = reinterpret_cast<ResultSet*>(cookie);

//     if (lcb_respstore_status(obresp) == LCB_SUCCESS) {
//         if (rflags & LCB_RESP_F_FINAL) {
//             if (out->options.persist != out->obs_persist_count) {
//                 fprintf(stderr, "Item persistence not matched Received %d Expected %d \n",
//                          out->obs_persist_count, out->options.persist);
//             }
//             if (out->options.replicate != out->obs_replica_count) {
//                 fprintf(stderr, "Item replication not matched Received %d Expected %d \n",
//                         out->obs_replica_count, out->options.replicate);
//             }
//             out->setRescode(lcb_respstore_status(obresp), key, nkey);
//         }
//         if (ismaster == 1) {
//             out->obs_persist_count++;
//             out->obs_master_cas = cas;
//             fprintf(stderr, "master cas %lu\n", (unsigned long)cas);
//         }

//         else if (lcb_respstore_status(obresp) == 1) {
//             if (cas == out->obs_master_cas) {
//                 out->obs_persist_count++;
//             } else {
//                 fprintf(stderr, "cas not matched master cas %lu  replica %lu \n",
//                         (unsigned long)out->obs_master_cas, (unsigned long)cas);
//             }
//             out->obs_replica_count++;
//         }
//     } else {
//         out->setRescode(lcb_respstore_status(obresp), key, nkey);
//     }
// }

} /* extern "C" */


Handle::Handle(const HandleOptions& opts) :
        options(opts),
        cluster(couchbase::cluster(io))
{
    io_thread = std::thread([this]() { io.run(); });
}


Handle::~Handle() {

     {
        auto barrier = std::make_shared<std::promise<std::error_code>>();
        auto f = barrier->get_future();
        cluster.close_bucket(options.bucket, [barrier](std::error_code ec) mutable { barrier->set_value(ec); });
        auto rc = f.get();
   }

    {
        auto barrier = std::make_shared<std::promise<void>>();
        auto f = barrier->get_future();
        cluster.close([barrier]() { barrier->set_value(); });
        f.get();
   }

    io_thread.join();

    destroy_logger();

}

bool
Handle::connect(Error *errp)
{
    // Gather parameters
    std::string connstr;

    if(options.useSSL) {
        connstr += std::string("couchbases://") + options.hostname;
        connstr += std::string("?certpath=");
        connstr += std::string(options.certpath);
        certpath = options.certpath;
    } else {
        connstr += std::string("couchbase://") + options.hostname;
    }

    create_logger(Daemon::MainDaemon->getOptions().lcblogFile);

    auto cb_connstr = couchbase::utils::parse_connection_string(connstr);

    couchbase::cluster_credentials auth{};
    auth.username = options.username;
    auth.password = options.password;
    
    auto origin = couchbase::origin(auth, cb_connstr);

    {
        auto barrier = std::make_shared<std::promise<std::error_code>>();
        auto f = barrier->get_future();
        cluster.open(origin, [barrier](std::error_code ec) mutable { barrier->set_value(ec); });
        auto rc = f.get();
        
        if (rc) {
            errp->errstr = rc.message();
            return false;
        }
   }

   {
        auto barrier = std::make_shared<std::promise<std::error_code>>();
        auto f = barrier->get_future();
        cluster.open_bucket(options.bucket, [barrier](std::error_code ec) mutable { barrier->set_value(ec); });
        auto rc = f.get();
        if (rc) {
            errp->errstr = rc.message();
            return false;
        }
   }

   // ping all nodes to work around defer issue CXXCBC-46
   while (true)
   {
        auto barrier = std::make_shared<std::promise<couchbase::diag::ping_result>>();
        auto f = barrier->get_future();
        cluster.ping(
          "my_report_id", {}, {couchbase::service_type::key_value}, [barrier](couchbase::diag::ping_result&& resp) mutable { barrier->set_value(std::move(resp)); });
        auto resp = f.get();
        bool ok = true;
        for (const auto& [t, info] : resp.services) {
            if (t == couchbase::service_type::key_value) {
                for (const auto& i : info) {
                    if ( i.state != couchbase::diag::ping_state::ok) {
                        ok = false;
                    }
                }
            }
        }
        if (ok) {
            break;
        }
    }
    
    return true;
}

bool Handle::generateCollections() {
    if(options.useCollections){
        log_info("Creating collections.\n");
        return collections->getInstance().generateCollections(this, options.scopes, options.collections);
    }
    return false;
}

void
Handle::collect_result(ResultSet& rs)
{
    // Here we 'wait' for a result.. we might either wait after each
    // operation, or wait until we've accumulated all batches. It really
    // depends on the options.
    if (rs.remaining < 0) {
        log_error("Received extra callbacks");
    }
    if (!rs.remaining) {
        return;
    }
}

bool
Handle::postsubmit(ResultSet& rs, unsigned int nsubmit)
{
    rs.remaining += nsubmit;

    if (!rs.options.iterwait) {
        // everything is buffered up
        return true;
    }

    if (rs.remaining > 0 && static_cast<unsigned int>(rs.remaining) < rs.options.iterwait) {
        return true;
    }

    unsigned int wait_msec = rs.options.getDelay();

    if (wait_msec) {
        sdkd_millisleep(wait_msec);
    }

    // if (Daemon::MainDaemon->getOptions().noPersist) {
    //     // lcb_destroy(instance);
    //     Error e;
    //     connect(&e);
    // }
    return false;
}

bool
Handle::dsGet(Command cmd, Dataset const &ds, ResultSet& out,
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

        std::pair<string,string> collection = getCollection(k);

        {
            couchbase::document_id id(this->options.bucket, collection.first, collection.second, k);
            couchbase::operations::get_request req{ id };
            out.markBegin();
            postsubmit(out);
            auto resp = execute(req);
            out.setRescode(resp.ctx.ec, k.c_str(), k.size());
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
    do_cancel = false;

    if (cmd != Command::MC_DS_MUTATE_ADD &&
        cmd != Command::MC_DS_MUTATE_SET &&
        cmd != Command::MC_DS_MUTATE_REPLACE &&
        cmd != Command::MC_DS_MUTATE_APPEND &&
        cmd != Command::MC_DS_MUTATE_PREPEND) {
        out.oper_error = Error(Error::SUBSYSf_SDKD,
                               Error::SDKD_EINVAL,
                               "Unknown mutation operation");
        return false;
    }

    auto exp = out.options.expiry;
    DatasetIterator *iter = ds.getIter();

    for (iter->start();
            iter->done() == false && do_cancel == false;
            iter->advance()) {

        std::string k = iter->key();
        std::string v = iter->value();

        std::pair<string,string> collection = getCollection(k);

        couchbase::document_id id(this->options.bucket, collection.first, collection.second, k);

        out.markBegin();
        postsubmit(out);
        std::error_code ec;

        if (cmd == Command::MC_DS_MUTATE_SET) {
            couchbase::operations::upsert_request req{ id, v };
            req.expiry = exp;
            req.flags = FLAGS;
            auto resp = execute(req);
            ec = resp.ctx.ec;
        } else if (cmd == Command::MC_DS_MUTATE_ADD) {
            couchbase::operations::insert_request req{ id, v };
            req.expiry = exp;
            req.flags = FLAGS;
            auto resp = execute(req);
            ec = resp.ctx.ec;
        } else if (cmd == Command::MC_DS_MUTATE_APPEND) {
            couchbase::operations::append_request req{ id, v };
            auto resp = execute(req);
            ec = resp.ctx.ec;
        } else if (cmd == Command::MC_DS_MUTATE_PREPEND) {
            couchbase::operations::prepend_request req{ id, v };
            auto resp = execute(req);
            ec = resp.ctx.ec;
        } else if (cmd == Command::MC_DS_MUTATE_REPLACE) {
            couchbase::operations::replace_request req{ id, v };
            req.expiry = exp;
            req.flags = FLAGS;
            auto resp = execute(req);
            ec = resp.ctx.ec;
        }

        out.setRescode(ec, k.c_str(), k.length());

    }
    delete iter;
    collect_result(out);
    return true;
}

// bool
// Handle::dsGetReplica(Command cmd, Dataset const &ds, ResultSet& out,
//               const ResultOptions& options)
// {
//     out.options = options;
//     out.clear();
//     do_cancel = false;

//     DatasetIterator* iter = ds.getIter();

//     for (iter->start();
//             iter->done() == false && do_cancel == false;
//             iter->advance()) {

//         std::string k = iter->key();
//         std::pair<string,string> collection = getCollection(k);
//         log_trace("GET REPLICA : %s", k.c_str());

//         lcb_CMDGETREPLICA *cmd;
//         lcb_cmdgetreplica_create(&cmd, LCB_REPLICA_MODE_ANY);
//         if(collection.first.length() != 0) {
//             lcb_cmdgetreplica_collection(cmd, collection.first.c_str(), collection.first.size(), collection.second.c_str(), collection.second.size());;
//         }
//         lcb_cmdgetreplica_key(cmd, k.data(), k.size());

//         out.markBegin();
//         lcb_STATUS err = lcb_getreplica(instance, &out, cmd);
//         lcb_cmdgetreplica_destroy(cmd);

//         if (err == LCB_SUCCESS) {
//             postsubmit(out);
//         } else {
//             out.setRescode(err, k, true);
//         }
//     }

//     delete iter;
//     collect_result(out);
//     return true;
// }

// bool
// Handle::dsEndure(Command cmd, Dataset const &ds, ResultSet& out,
//         const ResultOptions& options)
// {
//     out.options = options;
//     out.clear();
//     do_cancel = false;

//     DatasetIterator* iter = ds.getIter();

//     for (iter->start();
//             iter->done() == false && do_cancel == false;
//             iter->advance()) {

//         std::string k = iter->key(), v = iter->value();
//         std::pair<string,string> collection = getCollection(k);

//         lcb_CMDSTORE *cmd;
//         lcb_cmdstore_create(&cmd, LCB_STORE_UPSERT);
//         if(collection.first.length() != 0) {
//             lcb_cmdstore_collection(cmd, collection.first.c_str(), collection.first.size(), collection.second.c_str(), collection.second.size());
//         }
//         lcb_cmdstore_key(cmd, k.data(), k.size());
//         lcb_cmdstore_value(cmd, v.data(), v.size());
//         lcb_cmdstore_durability_observe(cmd, options.persist, options.replicate);

//         out.markBegin();

//         lcb_STATUS err = lcb_store(instance, &out, cmd);
//         lcb_cmdstore_destroy(cmd);

//         if (err == LCB_SUCCESS) {
//             postsubmit(out);
//         } else {
//             out.setRescode(err, k, true);
//         }
//     }

//     delete iter;
//     collect_result(out);
//     return true;
// }

// bool
// Handle::dsObserve(Command cmd, Dataset const &ds, ResultSet& out,
//               const ResultOptions& options)
// {
//     out.options = options;
//     out.clear();
//     do_cancel = false;

//     DatasetIterator* iter = ds.getIter();

//     for (iter->start();
//             iter->done() == false && do_cancel == false;
//             iter->advance()) {

//         std::string k = iter->key();
//         std::pair<string,string> collection = getCollection(k);

//         lcb_CMDSTORE *cmd;
//         lcb_cmdstore_create(&cmd, LCB_STORE_UPSERT);
//         if(collection.first.length() != 0) {
//             lcb_cmdstore_collection(cmd, collection.first.c_str(), collection.first.size(), collection.second.c_str(), collection.second.size());
//         }
//         lcb_cmdstore_key(cmd, k.c_str(), k.size());

//         out.markBegin();

//         lcb_STATUS err = lcb_store(instance, &out, cmd);
//         lcb_cmdstore_destroy(cmd);

//         out.obs_persist_count = 0;
//         out.obs_replica_count = 0;
//         if (err == LCB_SUCCESS) {
//             postsubmit(out);
//         } else {
//             out.setRescode(err, k, true);
//         }
//     }

//     delete iter;
//     collect_result(out);
//     return true;
// }



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
        std::pair<string,string> collection = getCollection(k);

        couchbase::document_id id(this->options.bucket, collection.first, collection.second, k);

        out.markBegin();
        postsubmit(out);
        std::error_code ec;

        if (cmd == Command::MC_DS_DELETE) {
            couchbase::operations::remove_request req{ id };
            auto resp = execute(req);
            ec = resp.ctx.ec;
        } else {
            couchbase::operations::touch_request req{ id };
            auto resp = execute(req);
            ec = resp.ctx.ec;
        }

        out.setRescode(ec, k.c_str(), k.length());
    }
    delete iter;
    collect_result(out);
    return true;
}

bool
Handle::dsSDSinglePath(Command c, const Dataset& ds, ResultSet& out, const ResultOptions& options)
{
    out.options = options;
    out.clear();
    DatasetIterator* iter = ds.getIter();
    do_cancel = false;

    for (iter->start(); !iter->done() && !do_cancel; iter->advance()) {
        std::string key = iter->key();
        std::string path = iter->path();
        std::string value = iter->value();
        std::string command = iter->command();
        std::pair<string, string> collection = getCollection(key);
        couchbase::document_id id(this->options.bucket, collection.first, collection.second, key);

        out.markBegin();
        postsubmit(out);
        std::error_code ec;

        if (command == "get") {
            couchbase::operations::lookup_in_request req{ id };
            req.specs.add_spec(couchbase::protocol::subdoc_opcode::get, false, path);
            auto resp = execute(req);
            ec = resp.ctx.ec;
        } else if (command == "get_multi") {
            couchbase::operations::lookup_in_request req{ id };
            req.specs.add_spec(couchbase::protocol::subdoc_opcode::get, false, path);
            req.specs.add_spec(couchbase::protocol::subdoc_opcode::exists, false, path);
            auto resp = execute(req);
            ec = resp.ctx.ec;
        } else if (command == "replace") {
            couchbase::operations::mutate_in_request req{ id };
            req.specs.add_spec(couchbase::protocol::subdoc_opcode::replace, false, false, false, path, value);
            auto resp = execute(req);
            ec = resp.ctx.ec;
        } else if (command == "dict_add") {
            couchbase::operations::mutate_in_request req{ id };
            req.specs.add_spec(couchbase::protocol::subdoc_opcode::dict_add, false, false, false, path, value);
            auto resp = execute(req);
            ec = resp.ctx.ec;
        } else if (command == "dict_upsert") {
            couchbase::operations::mutate_in_request req{ id };
            req.specs.add_spec(couchbase::protocol::subdoc_opcode::dict_upsert, false, false, false, path, value);
            auto resp = execute(req);
            ec = resp.ctx.ec;
        } else if (command == "array_add") {
            couchbase::operations::mutate_in_request req{ id };
            req.specs.add_spec(couchbase::protocol::subdoc_opcode::array_push_first, false, false, false, path, value);
            auto resp = execute(req);
            ec = resp.ctx.ec;
        } else if (command == "array_add_last") {
            couchbase::operations::mutate_in_request req{ id };
            req.specs.add_spec(couchbase::protocol::subdoc_opcode::array_push_last, false, false, false, path, value);
            auto resp = execute(req);
            ec = resp.ctx.ec;
        } else if (command == "counter") {
            couchbase::operations::mutate_in_request req{ id };
            req.specs.add_spec(couchbase::protocol::subdoc_opcode::counter, false, false, false, path, value);
            auto resp = execute(req);
            ec = resp.ctx.ec;
        }

        out.setRescode(ec, key.c_str(), key.length());
    }

    delete iter;
    collect_result(out);
    return true;
}

void
Handle::cancelCurrent()
{
    do_cancel = true;
    //delete certfile if exists
    if (certpath.size()) {
        remove(certpath.c_str());
    }
}

//Get scope name and collection name to use from the key
//We expect keys with a trailing numeric part "SimpleKeyREP11155REP11155REP11155", "key5", "24", etc.
std::pair<string, string> Handle::getCollection(const std::string key) {
    std::pair<string, string> coll("_default", "_default");//Converts to default collection
    if(options.useCollections && !key.empty()){
        //Defaults
        coll.first = "0";
        coll.second = "0";
        std::string last_n = key.substr(max(0, (int)key.length() - 3));//Last 3 chars or whole string
        int key_num = std::stoi(std::regex_replace(last_n, std::regex(R"([\D])"), ""));//Remove any remaining non-numeric chars
        int collection_num  = key_num % (options.collections * options.scopes);
        int scope_num = floor((float)collection_num / (float)options.collections);

        coll.first  = to_string(scope_num);
        coll.second = to_string(collection_num);
    }
    return coll;
}

} /* namespace CBSdkd */
