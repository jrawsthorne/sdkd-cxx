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

#include "Dataset.h"
#include "Request.h"
#include "Response.h"
#include "Handle.h"


namespace CBSdkd {

class IODispatch {

public:
    IODispatch();
    virtual ~IODispatch();

    virtual void run() = 0;

protected:
    int sockfd;
    // Store the 'raw' unparsed input buffer
    std::string inbuf;

    // line-based input queue
    std::list<std::string> newlines;

    Request* readRequest(bool do_block = false);
    void writeResponse(const Response& res);

private:
    Request * _get_single_request();
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
};

class WorkerDispatch : public IODispatch {

public:

    WorkerDispatch(int newsock, MainDispatch* parent);

    virtual ~WorkerDispatch() { }
    void run();

    pthread_t thr;

private:
    MainDispatch *parent;
    bool is_alive;

    static void *
    pthr_run(WorkerDispatch *w);
};

} /* namespace CBSdkd */
#endif /* IODISPATCH_H_ */
