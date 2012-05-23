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

class MainDispatch : public IODispatch {

public:
    MainDispatch();
    virtual ~MainDispatch();

    bool establishSocket(struct sockaddr_in *addr);
    void run();

    const Dataset* getDatasetById(const std::string& dsid);

private:
    int acceptfd;
    void
    create_new_ds(const Request* req);

    std::map<std::string,Dataset*> dsmap;
    std::list<WorkerDispatch*> children;

    pthread_mutex_t dsmutex;
    void _collect_workers();
};

class WorkerDispatch : protected IODispatch {

public:
    friend class MainDispatch;
    WorkerDispatch(int newsock, MainDispatch* parent);
    virtual ~WorkerDispatch();

    void run();

    pthread_t thr;

private:
    MainDispatch *parent;
    bool is_alive;
    char *friendly_cstr;
    static void *
    pthr_run(WorkerDispatch *w);
};

} /* namespace CBSdkd */
#endif /* IODISPATCH_H_ */
