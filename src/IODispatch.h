/*
 * IODispatch.h
 *
 *  Created on: May 12, 2012
 *      Author: mnunberg
 */

#ifndef IODISPATCH_H_
#define IODISPATCH_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>

#include "Dataset.h"
#include "Request.h"
#include "Response.h"
#include "Handle.h"
#include "contrib/debug++.h"
#include <cstdio>


namespace CBSdkd {

typedef unsigned long int cbsdk_hid_t;

class IOProtoHandler : protected DebugContext {
public:
    enum IOStatus {
        OK,
        NOTYET,
        ERR
    };

    IOProtoHandler(int newsock) : sockfd(newsock) { }
    IOProtoHandler() : sockfd(-1), inbuf("") { }

    virtual ~IOProtoHandler()  {
        if (sockfd >= 0) {
            close(sockfd);
            sockfd = -1;
        }
    }

    IOStatus getRawMessage(std::string& msgbuf, bool do_block = true);
    IOStatus putRawMessage(const std::string& msgbuf, bool do_block = true);

    void writeResponse(const Response& res);
    Request* readRequest(bool do_block = false, Request* reqp = NULL);

protected:
    int sockfd;
    std::string inbuf;
    std::list<std::string> newlines;
};

class IODispatch : protected IOProtoHandler {

public:
    IODispatch();
    virtual ~IODispatch();

    virtual void run() = 0;

};

class MainDispatch;
class WorkerDispatch;
class WorkerHandle;

class MainDispatch : public IODispatch {

public:
    MainDispatch();
    virtual ~MainDispatch();

    bool establishSocket(struct sockaddr_in *addr);
    void run();

    void registerWDHandle(cbsdk_hid_t, WorkerDispatch*);
    void unregisterWDHandle(cbsdk_hid_t);

    const Dataset* getDatasetById(const std::string& dsid);

private:
    int acceptfd;
    void
    create_new_ds(const Request* req);

    // Map from a DSID to a Dataset
    std::map<std::string,Dataset*> dsmap;

    // Map from a Handle ID to a handle
    std::map<cbsdk_hid_t, WorkerDispatch*> h2wmap;

    std::list<WorkerDispatch*> children;

    pthread_mutex_t dsmutex;
    pthread_mutex_t wmutex;

    void _collect_workers();
    const Handle* _get_handle(cbsdk_hid_t);

    void dispatch_cancel(const Request&);
};

class WorkerDispatch : protected IODispatch {

public:
    friend class MainDispatch;
    WorkerDispatch(int newsock, MainDispatch* parent);
    virtual ~WorkerDispatch();

    void run();

    // Cancels the current running handle, if any
    void cancelCurrentHandle();

    pthread_t thr;
    unsigned int id;

private:
    MainDispatch *parent;
    bool is_alive;
    char *friendly_cstr;

    Handle *cur_handle;
    cbsdk_hid_t cur_hid;

    bool _process_request(const Request&, ResultSet*);

    static void *pthr_run(WorkerDispatch *w);

    pthread_mutex_t hmutex;
};

} /* namespace CBSdkd */
#endif /* IODISPATCH_H_ */
