/*
 * IODispatch.cpp
 *
 *  Created on: May 12, 2012
 *      Author: mnunberg
 */

#include "/home/mnunberg/src/cbsdkd/cplusplus/IODispatch.h"
#include <cstring>
#include <cstdlib>
#include <cassert>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <auto_ptr.h>
#include <signal.h>
#include "Response.h"

namespace CBSdkd {

IODispatch::IODispatch()
: sockfd(-1)
{
}

IODispatch::~IODispatch()
{
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
}

// Parse a single line, if it's a valid request, then return it.
// If it's not, secretly send an error.. and return nothing.
Request*
IODispatch::_get_single_request()
{
    if (!newlines.size()) {
        return NULL;
    }

    std::string line = newlines.front();
    if (!line.size()) {
        return NULL;
    }
    Request *ret = Request::decode(line, NULL, true);
    if (ret->isValid()) {
        return ret;
    }

    // It's bad!
    assert(ret->getError() );
    writeResponse( Response(ret, ret->getError()) );
    delete ret;

    return NULL;
}

Request *
IODispatch::readRequest(bool do_block)
{
    char buf[4096];
    int rv;
    int pos_begin = 0;
    int rflags = (do_block) ? MSG_DONTWAIT : 0;
    int do_read =  1;
    Request *ret;

    while (do_read) {

        while ( (rv = recv(sockfd, buf, 4096, rflags)) > 0 ) {

            for (int ii = 0; ii < rv; ii++) {
                if (buf[ii] == '\n') {

                    std::string tmpbuf = inbuf;
                    tmpbuf.append(buf, rv - ii);
                    newlines.push_back(tmpbuf);
                    inbuf = "";
                    pos_begin = ii;
                }
            }

            inbuf.append(buf + pos_begin, rv - pos_begin);

            if (do_block && (ret = _get_single_request())) {
                break;
            }

        }

        if (rv == 0) {
            close(sockfd);
            cerr << "Socket Closed\n";
            sockfd = -1;
            return NULL;
        }

        int errno_save = errno;
        if (errno_save == EAGAIN && do_block == false) {
            break;
        }

        if (errno_save == EINTR) {
            continue;
        }

    }
    if (!ret) {
        ret = _get_single_request();
    }
    return ret;
}

void
IODispatch::writeResponse(const Response& res)
{
    std::string outbuf = res.encode() + "\n";
    if (-1 == send(sockfd, outbuf.c_str(), outbuf.size(), 0)) {
        cerr << "Couldn't send! " << errno << endl;
    }
}


MainDispatch::MainDispatch() : IODispatch(), acceptfd(-1)
{
}

MainDispatch::~MainDispatch()
{
    if (acceptfd >= 0) {
        close(acceptfd);
        acceptfd = -1;
    }
}

bool
MainDispatch::establishSocket(struct sockaddr_in *addr)
{
    if (-1 == (acceptfd = socket(AF_INET, SOCK_STREAM, 0))) {
        return false;
    }

    struct sockaddr_in myaddr;
    memset(&myaddr, 0, sizeof(myaddr));

    if (-1 == bind(acceptfd, (struct sockaddr*)&myaddr, sizeof(myaddr)) ) {
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

    while (1) {
        memcpy(&rfds, &origfds, sizeof(origfds));
        int selv = select(fdmax, &rfds, NULL, NULL, NULL);
        if (-1 == selv) {
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
            for (std::list<WorkerDispatch*>::iterator iter =
                    children.begin(); iter != children.end(); iter++) {
                if (pthread_kill((*iter)->thr, 0)) {
                    pthread_join((*iter)->thr, NULL);
                    delete *iter;
                    children.remove(*iter);
                }
            }
            WorkerDispatch *w = new WorkerDispatch(newsock, this);
            children.push_back(w);

            pthread_create(&w->thr, NULL, new_worker_thread, w);
        } else {
            Request *reqp = readRequest();
            // Process request..
            if (!reqp) {
                continue;
            }

            if (reqp->command == Command::NEWDATASET) {
                create_new_ds(reqp);
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

WorkerDispatch::WorkerDispatch(int newsock, MainDispatch *parent)
: IODispatch(),
  parent(parent)
{
    sockfd = newsock;
}

// Worker does some stuff here.. dispatching commands to its own handle
// instance.
// This is actually two phases because we first need to wait and receive the
// initial control message..

void
WorkerDispatch::run() {
    Request *reqp = readRequest(true);
    if (!reqp) {
        cerr << "Couldn't negotiate initial request!\n";
        return;
    }

    Error errp;

    HandleOptions hopts = HandleOptions(reqp->payload);
    if (!hopts.isValid()) {
        writeResponse(Response(reqp, Error(Error::SUBSYSf_SDKD,
                                            Error::SDKD_EINVAL,
                                            "Problem with handle input params")));
    }
    Handle h(hopts);

    if (!h.connect(&errp)) {
        writeResponse(Response(reqp, errp));
        delete reqp;
        return;
    }

    // Notify a success
    writeResponse(Response(reqp));

    // Now start receiving responses, now that the handle has
    // been established..
    delete reqp;

    auto_ptr<ResultSet> rs(new ResultSet);
    while (1) {
        reqp = readRequest(true);
        if (!reqp) {
            cerr << "Problem reading request...?\n";
            return;
        }

        const Dataset *ds;
        Dataset::Type dstype;
        std::string refid;

        dstype = Dataset::determineType(*reqp, &refid);
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
            ds = Dataset::fromType(dstype, *reqp);
            if (ds == NULL) {
                errp.setCode(Error::SUBSYSf_SDKD|Error::SDKD_EINVAL);
                errp.setString("Bad dataset parameters");
                goto GT_CHECKERR;
            }
        }

        GT_CHECKERR:
        if (errp) {
            writeResponse(Response(reqp, errp));
            errp.clear();
            delete reqp;
            continue;
        }

        ResultOptions opts = ResultOptions(reqp->payload["Options"]);

        switch (reqp->command) {
        case Command::MC_DS_DELETE:
        case Command::MC_DS_TOUCH:
            h.dsKeyop(reqp->command, *ds, *rs, opts);
            break;

        case Command::MC_DS_MUTATE_ADD:
        case Command::MC_DS_MUTATE_REPLACE:
        case Command::MC_DS_MUTATE_APPEND:
        case Command::MC_DS_MUTATE_PREPEND:
        case Command::MC_DS_MUTATE_SET:
            h.dsMutate(reqp->command, *ds, *rs, opts);
            break;

        case Command::MC_DS_GET:
            h.dsGet(reqp->command, *ds, *rs, opts);
            break;

        default:
            writeResponse(Response(reqp, Error(Error::SUBSYSf_SDKD,
                                                Error::SDKD_ENOIMPL)));
            continue;
            break;
        }

        if (rs->getError()) {
            writeResponse(Response(reqp, rs->getError()));
        } else {
            Response *resp = new Response(reqp);
            Json::Value root;
            rs->resultsJson(&root);
            resp->setResponseData(root);
            writeResponse(*resp);
            delete resp;
        }
    }
}

} /* namespace CBSdkd */
