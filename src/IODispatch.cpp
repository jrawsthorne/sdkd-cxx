/*
 * IODispatch.cpp
 *
 *  Created on: May 12, 2012
 *      Author: mnunberg
 */

#include "sdkd_internal.h"
#include <time.h>
#include <signal.h>



namespace CBSdkd {

extern "C" {

static struct {
    pthread_mutex_t mutex;
    timer_t tmid;
    bool initialized;
} Global_Timer;

void sdkd_init_timer(void)
{
    pthread_mutex_init(&Global_Timer.mutex, NULL);
    Global_Timer.initialized = false;
}

static void ttl_expired_func(union sigval)
{
    fprintf(stderr, "TTL Timer expired. Abort\n");
    abort();
}

void sdkd_set_ttl(unsigned seconds)
{
    pthread_mutex_lock(&Global_Timer.mutex);
    if (Global_Timer.initialized) {
        timer_delete(Global_Timer.tmid);
        Global_Timer.initialized = false;
    }

    if (seconds) {
        struct sigevent sev;
        struct itimerspec its;
        int rv;

        memset(&sev, 0, sizeof(sev));
        memset(&its, 0, sizeof(its));

        log_noctx_info("Setting timer to %u seconds", seconds);


        sev.sigev_notify = SIGEV_THREAD;
        sev.sigev_notify_function = ttl_expired_func;
        rv = timer_create(CLOCK_REALTIME, &sev, &Global_Timer.tmid);
        assert(rv == 0);

        /**
         * Initialize the actual time..
         */
        its.it_value.tv_sec = seconds;
        rv = timer_settime(Global_Timer.tmid, 0, &its, NULL);
        assert(rv == 0);

        Global_Timer.initialized = 1;
    }
    pthread_mutex_unlock(&Global_Timer.mutex);
}

} //extern C

IODispatch::IODispatch()
: IOProtoHandler()
{
}

IODispatch::~IODispatch()
{
}

// Read a single line from the network, but don't return it.
// This will possibly do network I/O and return an unparsed buffer.
IOProtoHandler::IOStatus
IOProtoHandler::getRawMessage(std::string& msgbuf, bool do_block)
{
    if (newlines.size()) {
        msgbuf.assign(newlines.front());
        newlines.pop_front();
        if (msgbuf.size()) {
            return this->OK;
        }
    }

    char buf[4096];
    int rv, pos_begin = 0, rflags = (do_block) ? 0 : MSG_DONTWAIT, do_read = 1;
    int nmsgs = 0;

    while (do_read) {

        while ( (rv = recv(sockfd, buf, 4096, rflags)) > 0) {
            for (int ii = 0; ii < rv; ii++) {
                if (buf[ii] == '\n') {
                    std::string tmpbuf = inbuf;
                    tmpbuf.append(buf, ii+1);
                    newlines.push_back(tmpbuf);
                    inbuf.clear();
                    pos_begin = ii;
                    nmsgs++;
                }
            }
            inbuf.append(buf + pos_begin, rv - pos_begin);
            if (nmsgs) {
                do_read = false;
                break;
            }
        }

        if (rv == 0) {
            close(sockfd);
            log_warn("Remote closed the connection.. (without sending a CLOSE)");
            sockfd = -1;
            return this->ERR;
        }
        int errno_save = errno;
        if (errno_save == EAGAIN && do_block == false) {
            break;
        } else if (errno_save == EINTR) {
            continue;
        }
    }
    if (!nmsgs) {
        return this->NOTYET;
    } else {
        // Recurse
        return getRawMessage(msgbuf, do_block);
    }
}



Request *
IOProtoHandler::readRequest(bool do_block, Request *reqp)
{

    std::string reqbuf;
    int reqp_is_alloc = (reqp == NULL);

    if (getRawMessage(reqbuf, do_block) != this->OK) {
        return NULL;
    }
    if (reqp) {
        reqp->refreshWith(reqbuf, true);
    } else {
        reqp = Request::decode(reqbuf, NULL, true);
    }
    if (reqp->isValid()) {
        return reqp;
    }

    // It's bad!
    assert(reqp->getError() );
    writeResponse( Response(reqp, reqp->getError()) );
    if (reqp_is_alloc) {
        delete reqp;
    }
    return NULL;
}

IOProtoHandler::IOStatus
IOProtoHandler::putRawMessage(const string& msgbuf, bool do_block)
{
    std::string nlbuf = msgbuf + "\n";
    if (-1 == send(sockfd, msgbuf.c_str(), msgbuf.size(), 0)) {
        log_error("Couldn't send: %s", strerror(errno));
        return this->ERR;
    }
    return this->OK;
}

void
IOProtoHandler::writeResponse(const Response& res)
{
    std::string encoded = res.encode();
    log_debug("Encoded: %s\n", encoded.c_str());
    putRawMessage(encoded, true);
}


MainDispatch::MainDispatch() : IODispatch(), acceptfd(-1)
{
    setLogPrefix("LCB SDKD Control");
    int status;

#define _mutex_init_assert(cvar) \
    status = pthread_mutex_init(cvar, NULL); \
    if (status != 0) { \
        log_error("Couldn't initialize mutex %s: %s", #cvar, strerror(status)); \
        abort(); \
    }

    _mutex_init_assert(&dsmutex);
    _mutex_init_assert(&wmutex);

#undef _mutex_init_assert
}

MainDispatch::~MainDispatch()
{
    if (acceptfd >= 0) {
        close(acceptfd);
        acceptfd = -1;
    }

    log_info("Bye Bye!");
    while (children.size()) {
        for (std::list<WorkerDispatch*>::iterator iter = children.begin();
                iter != children.end(); iter++) {
//            pthread_kill( (*iter)->thr, SIGUSR1 );
            pthread_cancel((*iter)->thr);
        }
        _collect_workers();
    }

    pthread_mutex_destroy(&wmutex);
    pthread_mutex_destroy(&dsmutex);
}

void
MainDispatch::registerWDHandle(cbsdk_hid_t id, WorkerDispatch *d)
{
    pthread_mutex_lock(&wmutex);
    h2wmap[id] = d;
    pthread_mutex_unlock(&wmutex);
}

void
MainDispatch::unregisterWDHandle(cbsdk_hid_t id)
{
    pthread_mutex_lock(&wmutex);
    h2wmap.erase(id);
    pthread_mutex_unlock(&wmutex);
}

bool
MainDispatch::establishSocket(struct sockaddr_in *addr)
{
    if (-1 == (acceptfd = socket(AF_INET, SOCK_STREAM, 0))) {
        return false;
    }

    int optval = 1;
    setsockopt(acceptfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (-1 == bind(acceptfd, (struct sockaddr*)addr, sizeof(*addr)) ) {
        close(acceptfd);
        return false;
    }

    unsigned int addrlen = sizeof(*addr);
    if (-1 == getsockname(acceptfd, (struct sockaddr*)addr, &addrlen)) {
        close(acceptfd);
        return false;
    }

    if (-1 == listen(acceptfd, 5)) {
        close(acceptfd);
        return false;
    }

    return true;
}

static void *
new_worker_thread(void *worker)
{
    ((WorkerDispatch*)worker)->run();
    return NULL;
}

void
MainDispatch::_collect_workers()
{
    std::list<WorkerDispatch*>::iterator iter = children.begin();
    while (iter != children.end()) {
        if (pthread_kill((*iter)->thr, 0) != 0) {
            pthread_join( (*iter)->thr, NULL );
            log_debug("Joined thread '%s'", (*iter)->getLogPrefix().c_str());
            delete *iter;
            children.erase(iter++);
        } else {
            iter++;
        }
    }
}

void
MainDispatch::run()
{
    // So we've already established the socket..
    assert(acceptfd >= 0);
    fd_set rfds, origfds;

    FD_ZERO(&rfds);
    FD_ZERO(&origfds);

    struct sockaddr_in ctlconn;
    socklen_t addrlen = sizeof(ctlconn);

    if (-1 == ( sockfd = accept(acceptfd,
                                (struct sockaddr*)&ctlconn, &addrlen))) {
        return;  // Nothing to do here..
    }

    if (-1 == (fcntl(sockfd, F_SETFL,
                     fcntl(sockfd, F_GETFL)|O_NONBLOCK))) {
        return;
    }


    FD_SET(sockfd, &origfds);
    FD_SET(acceptfd, &origfds);
    int fdmax = max(acceptfd, sockfd) + 1;
    int status;

    while (1) {
        memcpy(&rfds, &origfds, sizeof(origfds));
        int selv = select(fdmax, &rfds, NULL, NULL, NULL);
        if (-1 == selv) {

            if (errno == EINTR) {
                continue;
            }
            log_warn("select: %d", strerror(errno));
            return;
        }
        assert(selv > 0);
        if (FD_ISSET(acceptfd, &rfds)) {
            // Establish new worker thread...
            int newsock = accept(acceptfd, NULL, NULL);
            if (newsock == -1) {
                return;
            }

            // Check any children that might need to be cleaned up:
            _collect_workers();
            WorkerDispatch *w = new WorkerDispatch(newsock, this);
            children.push_back(w);

            if (0 != (status =  pthread_create(&w->thr,
                                               NULL,
                                               new_worker_thread, w))) {
                log_error("Failed to create new worker thread: %s",
                        strerror(status));

            }
        } else {
            Request *reqp = readRequest();
            // Process request..
            if (!reqp) {
                continue;
            }

            if (reqp->command == Command::NEWDATASET) {
                create_new_ds(reqp);

            } else if (reqp->command == Command::GOODBYE) {
                delete reqp;
                return;

            } else if (reqp->command == Command::CANCEL) {
                WorkerDispatch *w = h2wmap[reqp->handle_id];
                if (!w) {
                    writeResponse(Response(reqp,
                                           Error(Error::SUBSYSf_SDKD,
                                                 Error::SDKD_ENOHANDLE)));
                } else {
                    w->cancelCurrentHandle();
                    writeResponse(Response(reqp, 0));
                }
            } else if (reqp->command == Command::INFO) {
                Response res = Response(reqp);
                Json::Value infores;
                Handle::VersionInfoJson(infores);
                res.setResponseData(infores);
                writeResponse(res);

            } else if (reqp->command == Command::TTL) {
                if (!reqp->payload.isMember(CBSDK_MSGFLD_TTL_SECONDS)) {
                    writeResponse(Response(reqp,
                                           Error::createInvalid(
                                                   "Missing Seconds")));
                } else {
                    int seconds = reqp->payload[CBSDK_MSGFLD_TTL_SECONDS].asInt();

                    if (seconds < 0) {
                        seconds = 0;
                    }

                    sdkd_set_ttl(seconds);
                    writeResponse(Response(reqp, Error(0)));
                }
            } else {
                // We don't currently support other types of control messages
                writeResponse(Response(reqp,
                                       Error(Error::SUBSYSf_SDKD,
                                             Error::SDKD_ENOIMPL)));
            }
            delete reqp;
        }
    }

}

void
MainDispatch::create_new_ds(const Request *reqp)
{
    // It only makes sense to construct an invidual dataset
    // if we have an ID by which to refer to it later on
    std::string refid = "";
    Dataset::Type dstype = Dataset::determineType(*reqp, &refid);
    if (dstype == Dataset::DSTYPE_INVALID) {
        Response resp = Response(reqp,
                                 Error(Error::SUBSYSf_SDKD,
                                       Error::SDKD_EINVAL,
                                       "Invalid Dataset"));
        writeResponse(resp);
    }

    Dataset *ds = Dataset::fromType(dstype, *reqp);

    if (ds && ds->isValid() && refid.size()) {
        // Insert the DS
        pthread_mutex_lock(&dsmutex);
        if (dsmap[refid]) {
            pthread_mutex_unlock(&dsmutex);
            writeResponse(Response(reqp,
                                   Error(Error::SUBSYSf_SDKD,
                                         Error::SDKD_ENODS,
                                         "Dataset already exists with this ID")));
            delete ds;
            return;
        }
        dsmap[refid] = ds;
        pthread_mutex_unlock(&dsmutex);

    } else {
        delete ds;
        writeResponse(Response(reqp,
                               Error(Error::SUBSYSf_SDKD,
                                     Error::SDKD_EINVAL,
                                     "NEWDATASET must have an ID")));
    }
}

const Dataset *
MainDispatch::getDatasetById(const std::string& dsid)
{
    pthread_mutex_lock(&dsmutex);
    const Dataset *ret = dsmap[dsid];
    pthread_mutex_unlock(&dsmutex);
    return ret;
}

WorkerDispatch::~WorkerDispatch()
{
    pthread_mutex_destroy(&hmutex);
    if (cur_handle) {
        delete cur_handle;
        cur_handle = NULL;
    }
    delete rs;
    rs = NULL;
}

WorkerDispatch::WorkerDispatch(int newsock, MainDispatch *parent)
: IODispatch(),
  parent(parent),
  cur_handle(NULL),
  cur_hid(0)
{
    sockfd = newsock;
    stringstream ss;
    ss << "lcb-sdkd-worker fd=" << newsock;
    setLogPrefix(ss.str());
    pthread_mutex_init(&hmutex, NULL);
    rs = new ResultSet();
}

// Return false when the main loop should terminate. True otherwise.
bool
WorkerDispatch::_process_request(const Request& req, ResultSet* rs)
{
    Error errp;
    const Dataset *ds;
    Dataset::Type dstype;
    std::string refid;
    Handle& h = *cur_handle;
    bool needs_ds = true;
    auto_ptr<const Dataset> ds_scopedel;
    ds_scopedel.reset();

    if (!req.isValid()) {
        log_warn("Got invalid request..");
        return false;
    }

    if (req.command == Command::CLOSEHANDLE) {
        log_info("CLOSEHANDLE called. Returning.");
        return false;
    }

    if (req.command == Command::CB_VIEW_QUERY) {
        needs_ds = false;
    }

    if (needs_ds) {
        dstype = Dataset::determineType(req, &refid);
        if (dstype == Dataset::DSTYPE_INVALID) {
            errp.setCode(Error::SUBSYSf_SDKD|Error::SDKD_EINVAL);
            errp.setString("Couldn't parse dataset");
            goto GT_CHECKERR;
        }

        if (dstype == Dataset::DSTYPE_REFERENCE) {
            // Assume it's valied
            ds = parent->getDatasetById(refid);
            if (!ds) {
                errp.setCode(Error::SUBSYSf_SDKD|Error::SDKD_ENODS);
                errp.setString("Couldn't find dataset");
                goto GT_CHECKERR;
            }
        } else {
            ds = Dataset::fromType(dstype, req);
            if (ds == NULL) {
                errp.setCode(Error::SUBSYSf_SDKD|Error::SDKD_EINVAL);
                errp.setString("Bad dataset parameters");
                goto GT_CHECKERR;
            }

            // Make the DS get cleaned up when we exit.
            ds_scopedel.reset(ds);
        }
    }

    GT_CHECKERR:
    if (errp) {
        writeResponse(Response(&req, errp));
        errp.clear();
        return true;
    }

    ResultOptions opts = ResultOptions(req.payload["Options"]);
    // There are some sanity checking operations we should perform.
    // Because we wrap the handle, and the handle cannot return any responses.

    // Continuous must be used with IterWait
    if (needs_ds && dstype == Dataset::DSTYPE_SEEDED) {
        if (opts.iterwait == false &&
                ((DatasetSeeded*)ds)->getSpec().continuous == true) {
            writeResponse(Response(&req, Error(Error::SUBSYSf_SDKD,
                                               Error::SDKD_EINVAL,
                                               "Continuous must be used with IterWait")));
            return true;
        }
    }

    log_debug("Command is %s (%d)", req.command.cmdstr.c_str(),
              req.command.code);

    switch (req.command) {
    case Command::MC_DS_DELETE:
    case Command::MC_DS_TOUCH:
        h.dsKeyop(req.command, *ds, *rs, opts);
        break;

    case Command::MC_DS_MUTATE_ADD:
    case Command::MC_DS_MUTATE_REPLACE:
    case Command::MC_DS_MUTATE_APPEND:
    case Command::MC_DS_MUTATE_PREPEND:
    case Command::MC_DS_MUTATE_SET:
        h.dsMutate(req.command, *ds, *rs, opts);
        break;

    case Command::MC_DS_GET:
        h.dsGet(req.command, *ds, *rs, opts);
        break;

    case Command::CB_VIEW_LOAD:
    {
        ViewLoader vl = ViewLoader(cur_handle);
        vl.populateViewData(req.command, *ds, *rs, opts, req);
        break;
    }

    case Command::CB_VIEW_QUERY:
    {
        ViewExecutor ve = ViewExecutor(cur_handle);
        ve.executeView(req.command, *rs, opts, req);
        break;
    }

    default:
        log_warn("Command '%s' not implemented",
                 req.command.cmdstr.c_str());
        writeResponse(Response(&req, Error(Error::SUBSYSf_SDKD,
                                            Error::SDKD_ENOIMPL)));
        return true;
    }

    if (rs->getError()) {
        writeResponse(Response(&req, rs->getError()));
    } else {
        Response resp = Response(&req);
        Json::Value root;
        rs->resultsJson(&root);
        resp.setResponseData(root);
        writeResponse(resp);
    }

    return true;
}

// Worker does some stuff here.. dispatching commands to its own handle
// instance.
// This is actually two phases because we first need to wait and receive the
// initial control message..
void
WorkerDispatch::run() {

    Request req;
    Error errp;
    HandleOptions hopts;

    readRequest(true, &req);

    if (!req.isValid()) {
        log_error("Couldn't negotiate initial request");
        goto GT_DONE;
    }

    hopts = HandleOptions(req.payload);
    if (!hopts.isValid()) {
        writeResponse(Response(&req, Error(Error::SUBSYSf_SDKD,
                                            Error::SDKD_EINVAL,
                                            "Problem with handle input params")));
    }

    cur_handle = new Handle(hopts);

    // We need to retain a pointer to our current handle (even if it lives
    // on the stack) in order to be able to send it signals from other
    // threads via cancelCurrentHandle().
    cur_hid = req.handle_id;

    parent->registerWDHandle(cur_hid, this);
    if (!cur_handle->connect(&errp)) {
        log_error("Couldn't establish initial control connection");
        writeResponse(Response(&req, errp));
        goto GT_DONE;
    }

    // Notify a success
    log_info("Successful handle established");
    writeResponse(Response(&req));

    // Now start receiving responses, now that the handle has
    // been established..


    while (true) {
        readRequest(true, &req);
        if (_process_request(req, rs) == false) {
            break;
        }
    }

    log_info("Loop done..");

    GT_DONE:
    if (cur_hid) {
        /* ... */
        parent->unregisterWDHandle(cur_hid);
        cur_hid = 0;
    }

    pthread_mutex_lock(&hmutex);
    if (cur_handle) {
        delete cur_handle;
        cur_handle = NULL;
    }
    pthread_mutex_unlock(&hmutex);
}

// We need significant indirection because of shared data access from multiple
// threads, and the fact that we want to make sure the handle itself is never
// destroyed from anything but the thread it was created in. (Some components
// may not be thread safe)
void
WorkerDispatch::cancelCurrentHandle()
{
    pthread_mutex_lock(&hmutex);
    if (cur_handle) {
        cur_handle->cancelCurrent();
    }
    pthread_mutex_unlock(&hmutex);
}

} /* namespace CBSdkd */
