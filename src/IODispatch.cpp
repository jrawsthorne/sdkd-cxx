/*
 * IODispatch.cpp
 *
 *  Created on: May 12, 2012
 *      Author: mnunberg
 */

#include "sdkd_internal.h"

namespace CBSdkd {

extern "C" {

static struct {
    Mutex *mutex;
    Timer *timer;
    bool initialized;
} Global_Timer;

void sdkd_init_timer(void)
{
    Global_Timer.mutex = Mutex::Create();
    Global_Timer.initialized = false;
}

static void ttl_expired_func(void *unused)
{
    fprintf(stderr, "TTL Timer expired. Abort\n");
    abort();
}

void sdkd_set_ttl(unsigned seconds)
{
    Global_Timer.mutex->lock();

    if (!Global_Timer.timer) {
        Global_Timer.timer = Timer::Create(ttl_expired_func);
    }

    if (Global_Timer.initialized) {
        Global_Timer.timer->disable();
        Global_Timer.initialized = false;
    }

    if (seconds) {
        Global_Timer.timer->schedule(seconds);
        Global_Timer.initialized = 1;
    }

    Global_Timer.mutex->unlock();
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
IOProtoHandler::getRawMessage(std::string& msgbuf)
{
    if (newlines.size()) {
        msgbuf.assign(newlines.front());
        newlines.pop_front();
        if (msgbuf.size()) {
            return this->OK;
        }
    }

    int rv = 0, pos_begin = 0,  do_read = 1;
    int nmsgs = 0;

    while (do_read) {
        char buf[4096] = { 0 };

        while ( (rv = recv(sockfd, buf, 4096, 0)) > 0) {
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
            closesocket(sockfd);
            log_warn("Remote closed the connection.. (without sending a CLOSE)");
            sockfd = -1;
            return this->ERR;

        } else if (rv == -1) {
            int errno_save = sdkd_socket_errno();
            if (errno_save == SDKD_SOCK_EWOULDBLOCK) {
                break;
            } else if (errno_save == SDKD_SOCK_EINTR) {
                continue;
            } else {
                log_warn("Got other socket error; sock=%d [%d]: %s", sockfd, errno_save, strerror(errno_save));
                return this->ERR;
            }
        }
    }
    if (!nmsgs) {
        return this->NOTYET;
    } else {
        // Recurse
        return getRawMessage(msgbuf);
    }
}



Request *
IOProtoHandler::readRequest(Request *reqp, bool do_loop)
{
    std::string reqbuf;
    int reqp_is_alloc = (reqp == NULL);
    IOStatus status;

    do {

        status = getRawMessage(reqbuf);

        if (status == OK) {
            break;

        } else if (status == ERR) {
            return NULL;
        }
        if (do_loop) {
            sdkd_millisleep(1);
        }
    } while (do_loop);

    if (status != OK) {
        /** NOTYET and do_loop == false */
        assert(status == NOTYET && do_loop == false);
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
IOProtoHandler::putRawMessage(const string& msgbuf)
{
    std::string nlbuf = msgbuf + "\n";
    int remaining = msgbuf.size();
    const char *sendbuf = msgbuf.c_str();

    while (remaining) {
        int rv = send(sockfd, sendbuf, remaining, 0);
        if (rv == -1) {
            int save_errno = sdkd_socket_errno();

            if (save_errno == SDKD_SOCK_EWOULDBLOCK) {
                return NOTYET;

            } else {
                log_error("Couldn't send: [%d], %s", save_errno, strerror(save_errno));
                return ERR;
            }
        }

        remaining -= rv;
        sendbuf += rv;
    }
    assert(remaining == 0);
    return OK;
}

void
IOProtoHandler::writeResponse(const Response& res)
{
    std::string encoded = res.encode();
    log_debug("Encoded: %s\n", encoded.c_str());
    sdkd_make_socket_nonblocking(sockfd, 0);
    putRawMessage(encoded);
    sdkd_make_socket_nonblocking(sockfd, 1);
}


MainDispatch::MainDispatch() : IODispatch(), acceptfd(-1)
{
    setLogPrefix("LCB SDKD Control");
    dsmutex = Mutex::Create();
    wmutex = Mutex::Create();
}

MainDispatch::~MainDispatch()
{
    if (acceptfd >= 0) {
        closesocket(acceptfd);
        acceptfd = -1;
    }

    log_info("Bye Bye!");
    while (children.size()) {
        for (std::list<WorkerDispatch*>::iterator iter = children.begin(); iter != children.end(); iter++) {
            (*iter)->cancelCurrentHandle();
        }
        _collect_workers();
    }

    delete wmutex;
    delete dsmutex;
}

void
MainDispatch::registerWDHandle(cbsdk_hid_t id, WorkerDispatch *d)
{
    wmutex->lock();
    h2wmap[id] = d;
    wmutex->unlock();
}

void
MainDispatch::unregisterWDHandle(cbsdk_hid_t id)
{
    wmutex->lock();
    h2wmap.erase(id);
    wmutex->unlock();
}

bool
MainDispatch::establishSocket(struct sockaddr_in *addr)
{
    acceptfd = sdkd_start_listening(addr);
    if (acceptfd == INVALID_SOCKET) {
        return false;
    }
    return true;
}

extern "C" {
#ifdef _WIN32
static unsigned __stdcall new_worker_thread(void *worker)
{
    reinterpret_cast<WorkerDispatch*>(worker)->run();
    return 0;
}
#else
static void *new_worker_thread(void *worker)
{
    ((WorkerDispatch*)worker)->run();
    return NULL;
}
#endif

}

void
MainDispatch::_collect_workers()
{
    std::list<WorkerDispatch*>::iterator iter = children.begin();
    while (iter != children.end()) {
        WorkerDispatch *w = *iter;
        if (!w->thr->isAlive()) {
            w->thr->join();

            log_debug("Joined thread '%s'", w->getLogPrefix().c_str());
            delete w;

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
    assert(acceptfd != INVALID_SOCKET);
    fd_set rfds, origfds;

    FD_ZERO(&rfds);
    FD_ZERO(&origfds);

    // The first control connection is special
    struct sockaddr_in ctlconn;
    if (INVALID_SOCKET == (sockfd = sdkd_accept_socket(acceptfd, &ctlconn))) {
        return;  // Nothing to do here..
    }

    if (SOCKET_ERROR == sdkd_make_socket_nonblocking(sockfd, 1)) {
        return;
    }

    if (SOCKET_ERROR == sdkd_make_socket_nonblocking(acceptfd, 1)) {
        return;
    }

    FD_SET(sockfd, &origfds);
    FD_SET(acceptfd, &origfds);
    sdkd_socket_t fdmax = max(acceptfd, sockfd) + 1;

    while (1) {
        memcpy(&rfds, &origfds, sizeof(origfds));
        int selv = select(fdmax, &rfds, NULL, NULL, NULL);
        if (SOCKET_ERROR == selv) {
            int errno_save = sdkd_socket_errno();
            if (errno_save == EINTR) {
                continue;
            }
            log_warn("select: %s", strerror(errno_save));
            return;
        }

        assert(selv != SOCKET_ERROR);

        if (FD_ISSET(acceptfd, &rfds)) {
            // Establish new worker thread...
            struct sockaddr_in inaddr;
            sdkd_socket_t newsock = sdkd_accept_socket(acceptfd, &inaddr);
            if (newsock == INVALID_SOCKET) {
                printf("Problem accepting new socket!\n");
                return;
            }

            // Check any children that might need to be cleaned up:
            WorkerDispatch *w = new WorkerDispatch(newsock, this);
            children.push_back(w);
            w->thr = Thread::Create(new_worker_thread);
            w->thr->start(w);

            _collect_workers();


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
        dsmutex->lock();
        if (dsmap[refid]) {
            dsmutex->unlock();
            writeResponse(Response(reqp,
                                   Error(Error::SUBSYSf_SDKD,
                                         Error::SDKD_ENODS,
                                         "Dataset already exists with this ID")));
            delete ds;
            return;
        }
        dsmap[refid] = ds;
        dsmutex->unlock();

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
    dsmutex->lock();
    const Dataset *ret = dsmap[dsid];
    dsmutex->unlock();
    return ret;
}


} /* namespace CBSdkd */
