#include "sdkd_internal.h"

namespace CBSdkd {

WorkerDispatch::~WorkerDispatch()
{
    if (cur_handle) {
        delete cur_handle;
        cur_handle = NULL;
    }
    delete rs;
    delete hmutex;
    rs = NULL;
}

WorkerDispatch::WorkerDispatch(sdkd_socket_t newsock, MainDispatch *parent)
: IODispatch(),
  parent(parent),
  cur_handle(NULL),
  cur_hid(0)
{
    sockfd = newsock;
    stringstream ss;
    ss << "lcb-sdkd-worker fd=" << newsock;
    setLogPrefix(ss.str());
    hmutex = Mutex::Create();
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
            abort();
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
    int rv = sdkd_make_socket_nonblocking(sockfd, 0);
    if (rv == -1) {
        perror("Couldn't make socket blocking\n");
    }

    readRequest(&req, true);

    if (!req.isValid()) {
        log_error("Couldn't negotiate initial request");
        goto GT_DONE;
    }

    hopts = HandleOptions(req.payload);
    if (!hopts.isValid()) {
        writeResponse(Response(&req, Error(Error::SUBSYSf_SDKD,
                                            Error::SDKD_EINVAL,
                                            "Problem with handle input params")));
        goto GT_DONE;
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
        Request *dummy = readRequest(&req, true);
        if (!dummy) {
            log_error("Probably got an I/O Error");
        }

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

    hmutex->lock();
    if (cur_handle) {
        delete cur_handle;
        cur_handle = NULL;
    }
    hmutex->unlock();
}

// We need significant indirection because of shared data access from multiple
// threads, and the fact that we want to make sure the handle itself is never
// destroyed from anything but the thread it was created in. (Some components
// may not be thread safe)
void
WorkerDispatch::cancelCurrentHandle()
{
    hmutex->lock();
    if (cur_handle) {
        cur_handle->cancelCurrent();
    }
    hmutex->unlock();
}

}
