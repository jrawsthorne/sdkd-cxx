#include "sdkd_internal.h"

namespace CBSdkd {

enum {
    VR_IX_IDENT = 0,
    VR_IX_INFLATEBASE = 1,
    VR_IX_INFLATECOUNT = 2,
    VR_IX_INFLATEBLOB = 3,
    VR_IX_MAX = 4
};

set<string> ViewExecutor::ViewOptions;

void
ViewExecutor::InitializeViewOptions()
{
#define XX(b, s, t) ViewExecutor::ViewOptions.insert(s);

    LCB_XVOPT

#undef XX
}

ViewExecutor::ViewExecutor(Handle *handle)
{
    this->handle = handle;
    rctx = lcb_vrow_create();
}

ViewExecutor::~ViewExecutor()
{
    lcb_vrow_free(rctx);
    rctx = NULL;
}


bool
ViewExecutor::genQueryString(const Request& req, string& out, Error& eo)
{
    vector<lcb_vopt_t> view_options;
    vector<lcb_vopt_t*> view_options_pointers;

    Json::Value vqopts = req.payload[CBSDKD_MSGFLD_V_QOPTS];
    string dname = req.payload[CBSDKD_MSGFLD_V_DESNAME].asString();
    string vname = req.payload[CBSDKD_MSGFLD_V_MRNAME].asString();
    bool ret = true;



    if (dname.size() == 0 || vname.size() == 0) {
        eo = Error(Error::createInvalid("Missing view or design name"));
        return false;
    }



    for (Json::ValueIterator iter = vqopts.begin();
            iter != vqopts.end();
            iter++) {

        if (ViewExecutor::ViewOptions.find(iter.key().asString()) ==
                ViewExecutor::ViewOptions.end()) {
            log_warn("Unknown query option '%s'", iter.key().asCString());
            continue;
        }

        lcb_vopt_t vopt;
        int ival;
        string sval;
        int jflags = 0;
        const void *val_arg;
        char *errstr;


        switch ((*iter).type()) {

        case Json::intValue:
        case Json::uintValue:
        case Json::realValue:
        case Json::nullValue:

            ival = (*iter).asInt();
            jflags = LCB_VOPT_F_OPTVAL_NUMERIC;
            val_arg = &ival;
            break;

        default:
            sval = (*iter).asString();
            val_arg = sval.c_str();
            break;

        }

        lcb_error_t err = lcb_vopt_assign(&vopt,
                                          iter.key().asString().c_str(),
                                          -1,
                                          val_arg,
                                          -1,
                                          jflags,
                                          &errstr);

        if (err != LCB_SUCCESS) {

            string errstr = "Bad value for option " + iter.key().asString();
            errstr += ": ";
            errstr += errstr;
            eo = Error(Error::createInvalid(errstr));
            lcb_vopt_cleanup(&vopt);
            ret = false;
            goto GT_DONE;
        }

        view_options.push_back(vopt);
    }

    for (unsigned int ii = 0; ii < view_options.size(); ii++) {
        view_options_pointers.push_back(&view_options[ii]);
    }


    if (view_options_pointers.size()) {
        char *vqstr;
        const lcb_vopt_t * const * tmp_pp =
                (const lcb_vopt_t* const *) &view_options_pointers[0];

        vqstr = lcb_vqstr_make_uri(dname.c_str(), dname.length(),
                                   vname.c_str(), vname.length(),
                                   tmp_pp, view_options_pointers.size());
        out.assign(vqstr);
        log_info("Generated query string %s", vqstr);
        free(vqstr);
    }


    GT_DONE:
    for (vector<lcb_vopt_t*>::iterator iter = view_options_pointers.begin();
            iter != view_options_pointers.end();
            iter++) {

        lcb_vopt_cleanup(*iter);
    }

    return ret;

}

extern "C" {
static void
row_callback(lcb_vrow_ctx_t *ctx,
             const void *cookie,
             const lcb_vrow_datum_t *res)
{
    /**
     * In here we ensure the row data is consistent
     */
    reinterpret_cast<ViewExecutor*>((void*)cookie)->handleRowResult(res);
}

static void
data_callback(lcb_http_request_t request,
              lcb_t instance,
              const void *cookie,
              lcb_error_t err,
              const lcb_http_resp_t *resp)
{
    ViewExecutor *vo = reinterpret_cast<ViewExecutor*>((void*)cookie);
    if (!vo->handleHttpChunk(err, resp)) {
        lcb_cancel_http_request(instance, request);
    }
}

}

bool
ViewExecutor::handleHttpChunk(lcb_error_t err, const lcb_http_resp_t *resp)
{
    if (!responseTick) {
        if (resp->v.v0.status < 200 || resp->v.v0.status > 299) {
            log_info("Got http code %d", resp->v.v0.status);
            rs->setRescode(Error(Error::SUBSYSf_VIEWS,
                                 Error::VIEWS_HTTP_ERROR));
            responseTick = true;
            return false;

        } else {
            rs->setRescode(0);
        }
    }

    responseTick = true;

    if (err != LCB_SUCCESS) {
        log_warn("LCB Returned code %d (%s) for http",
                 err,
                 lcb_strerror(handle->getLcb(), err));

        rs->setRescode(err, "", 0);
        return false;
    }

    // callout to vrow
    if (resp->v.v0.nbytes) {
        lcb_vrow_feed(rctx,
                      (const char*)resp->v.v0.bytes,
                      resp->v.v0.nbytes);
    }
    return true;
}

void
ViewExecutor::handleRowResult(const lcb_vrow_datum_t *dt)
{
    if (dt->type == LCB_VROW_COMPLETE) {
        return;

    } else if (dt->type == LCB_VROW_ERROR) {
        rs->setRescode(Error(Error::SUBSYSf_VIEWS, Error::VIEWS_MALFORMED));
        return;
    }

    persistRow.clear();
    persistKey.clear();

    jreader.parse(dt->data, dt->data + dt->ndata, persistRow, false);
    string id;

    if (!persistRow) {
        rs->setRescode(Error(Error::SUBSYSf_VIEWS, Error::VIEWS_MALFORMED));
        return;
    }

    persistKey = persistRow["key"];
    id = persistRow["id"].asString();

    if ( (!persistKey) || (!id.size()) ) {
        rs->setRescode(Error(Error::SUBSYSf_VIEWS, Error::VIEWS_MISMATCH));
        return;
    }

    string kvident = persistKey[VR_IX_IDENT].asString();
    if (kvident == id) {
        rs->setRescode(0);

    } else {
        rs->setRescode(Error(Error::SUBSYSf_VIEWS, Error::VIEWS_MISMATCH));
    }
}

void
ViewExecutor::runSingleView(const lcb_http_cmd_t *htcmd)
{
    lcb_http_request_t htreq;
    lcb_error_t lcb_err;

    responseTick = false;

    lcb_err = lcb_make_http_request(handle->getLcb(),
                                    this,
                                    LCB_HTTP_TYPE_VIEW,
                                    htcmd,
                                    &htreq);

    if (lcb_err != LCB_SUCCESS) {
        goto GT_ERR;
    }

    lcb_err = lcb_wait(handle->getLcb());

    if (!responseTick) {
        fprintf(stderr, "Wait returned prematurely. Abort\n");
        abort();
    }

    GT_ERR:
    if (lcb_err != LCB_SUCCESS) {
        // mark the actual error
        rs->setRescode(lcb_err, "", 0);
        // mark the schedule phase
        rs->setRescode(Error(Error::SUBSYSf_CLIENT, Error::CLIENT_ESCHED));
    }
}

bool
ViewExecutor::executeView(Command cmd,
                          ResultSet& out,
                          const ResultOptions& options,
                          const Request& req)
{
    string qstr;
    Error sdkd_err = 0;

    lcb_http_cmd_t htcmd = lcb_http_cmd_st();

    out.options = options;
    out.clear();
    this->rs = &out;


    Json::Value ctlopts = req.payload[CBSDKD_MSGFLD_DSREQ_OPTS];
    int num_iterations = ctlopts[CBSDKD_MSGFLD_V_QITERCOUNT].asInt();
    int iterdelay = ctlopts[CBSDKD_MSGFLD_V_QDELAY].asInt();

    if (!genQueryString(req, qstr, sdkd_err)) {
        log_error("Couldn't generate query string..");
        rs->setError(sdkd_err);
        return false;
    }

    htcmd.v.v0.chunked = 1;
    htcmd.v.v0.content_type = "application/json";
    htcmd.v.v0.path = qstr.c_str();
    htcmd.v.v0.npath = qstr.length();
    htcmd.v.v0.method = LCB_HTTP_METHOD_GET;

    lcb_http_data_callback old_data_cb =
            lcb_set_http_data_callback(handle->getLcb(),
                                       data_callback);
    lcb_http_complete_callback old_complete_cb =
            lcb_set_http_complete_callback(handle->getLcb(),
                                           data_callback);

    rctx->callback = row_callback;
    rctx->user_cookie = this;

    handle->externalEnter();

    while (!handle->isCancelled()) {

        rs->markBegin();
        runSingleView(&htcmd);

        if (num_iterations >= 0) {
            num_iterations--;
            if (num_iterations <= 0) {
                break;
            }
        }

        if (iterdelay) {
            sdkd_millisleep(iterdelay);
        }

        lcb_vrow_reset(rctx);
    }

    handle->externalLeave();

    lcb_set_http_data_callback(handle->getLcb(), old_data_cb);
    lcb_set_http_complete_callback(handle->getLcb(), old_complete_cb);

    return true;
}
}
