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
ViewExecutor::genOptionsString(const Request& req, string& out, Error& eo)
{
    vector<lcb_vopt_t> view_options;
    vector<lcb_vopt_t*> view_options_pointers;

    Json::Value vqopts = req.payload[CBSDKD_MSGFLD_V_QOPTS];
    bool ret = true;

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

        lcb_STATUS err = lcb_vopt_assign(&vopt,
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

    for (auto &view_option : view_options) {
        view_options_pointers.push_back(&view_option);
    }


    if (view_options_pointers.size()) {
        char *vqstr;
        const lcb_vopt_t * const * tmp_pp = (const lcb_vopt_t* const *)&view_options_pointers[0];
        vqstr = lcb_vqstr_make_optstr(tmp_pp, view_options_pointers.size());
        out.assign(vqstr);
        free(vqstr);
    }


    GT_DONE:
    for (auto &view_options_pointer : view_options_pointers) {
        lcb_vopt_cleanup(view_options_pointer);
    }

    return ret;
}



extern "C" {
void
dump_http_error(const lcb_RESPVIEW *resp) {
  const lcb_VIEW_ERROR_CONTEXT *ctx;
  lcb_respview_error_context(resp, &ctx);

  const char *endpoint = nullptr;
  size_t endpoint_len = 0;
  lcb_errctx_view_endpoint(ctx, &endpoint, &endpoint_len);

  uint32_t http_code = 0;
  lcb_errctx_view_http_response_code(ctx, &http_code);

  const char *err_code = nullptr;
  size_t err_code_len = 0;
  lcb_errctx_view_first_error_code(ctx, &err_code, &err_code_len);
  const char *err_msg = nullptr;
  size_t err_msg_len = 0;
  lcb_errctx_view_first_error_message(ctx, &err_msg, &err_msg_len);

  const char *design_document = nullptr;
  size_t design_document_len = 0;
  lcb_errctx_view_design_document(ctx, &design_document, &design_document_len);

  const char *view_name = nullptr;
  size_t view_name_len = 0;
  lcb_errctx_view_view(ctx, &view_name, &view_name_len);

  fprintf(stderr,
          "Failed to execute view. lcb: %s, endpoint: %.*s, http_code: %d, "
          "view_code: %.*s (%.*s), design_document: %.*s, view_name: %.*s\n",
          lcb_strerror_short(lcb_respview_status(resp)), (int)endpoint_len,
          endpoint, http_code, (int)err_code_len, err_code, (int)err_msg_len, err_msg,
          (int)design_document_len, design_document, (int)view_name_len, view_name);
}


static void rowCallback(lcb_INSTANCE *instance, int, const lcb_RESPVIEW *response) {
    void *cookie;
    lcb_respview_cookie(response, &cookie);
    if (lcb_respview_is_final(response)) {
        if (lcb_respview_status(response) != LCB_SUCCESS) {
            dump_http_error(response);
        }
        reinterpret_cast<ResultSet*>(cookie)->setRescode(lcb_respview_status(response), true);
        return;
    }
    reinterpret_cast<ResultSet*>(cookie)->setRescode(lcb_respview_status(response));
}

}
void
ViewExecutor::runSingleView(lcb_CMDVIEW *cmd, ResultSet& out)
{
    lcb_STATUS lcb_err;

    lcb_err = lcb_view(handle->getLcb(),
            &out, cmd);

    if (lcb_err != LCB_SUCCESS) {
        goto GT_ERR;
    }

    lcb_wait(handle->getLcb(), LCB_WAIT_NOCHECK);
    if (!out.vresp_complete) {
        fprintf(stderr, "Final view response not received..aborting");
        abort();
    }

GT_ERR:
    if (lcb_err != LCB_SUCCESS) {
        // mark the actual error
        rs->setRescode(lcb_err);
    }
}

bool
ViewExecutor::executeView(Command cmd,
                          ResultSet& out,
                          const ResultOptions& options,
                          const Request& req)
{
    string optstr;
    Error sdkd_err = 0;

    out.options = options;
    out.clear();
    this->rs = &out;

    Json::Value ctlopts = req.payload[CBSDKD_MSGFLD_DSREQ_OPTS];
    int num_iterations = ctlopts[CBSDKD_MSGFLD_V_QITERCOUNT].asInt();
    int iterdelay = ctlopts[CBSDKD_MSGFLD_V_QDELAY].asInt();

    string dname = req.payload[CBSDKD_MSGFLD_V_DESNAME].asString();
    string vname = req.payload[CBSDKD_MSGFLD_V_MRNAME].asString();

    if (dname.size() == 0 || vname.size() == 0) {
        log_error("Design/ view names cannot be empty");
        return false;
    }

    if (!genOptionsString(req, optstr, sdkd_err)) {
        log_error("Couldn't generate query string..");
        rs->setError(sdkd_err);
        return false;
    }

    lcb_CMDVIEW *vcmd;
    lcb_cmdview_create(&vcmd);
    lcb_cmdview_design_document(vcmd, dname.c_str(), dname.size());
    lcb_cmdview_view_name(vcmd, vname.c_str(), vname.size());
    lcb_cmdview_option_string(vcmd, optstr.c_str(), optstr.size());
    lcb_cmdview_callback(vcmd, rowCallback);

    handle->externalEnter();

    while (!handle->isCancelled()) {
        out.vresp_complete = false;

        rs->markBegin();
        runSingleView(vcmd, out);

        if (num_iterations >= 0) {
            num_iterations--;
            if (num_iterations <= 0) {
                break;
            }
        }

        if (iterdelay) {
            sdkd_millisleep(iterdelay);
        }

    }

    lcb_cmdview_destroy(vcmd);
    handle->externalLeave();
    return true;
}
}
