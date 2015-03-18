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
        const lcb_vopt_t * const * tmp_pp = (const lcb_vopt_t* const *)&view_options_pointers[0];
        vqstr = lcb_vqstr_make_optstr(tmp_pp, view_options_pointers.size());
        out.assign(vqstr);
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
static void rowCallback(lcb_t instance, int, const lcb_RESPVIEWQUERY *response) {
    if (response->rc == LCB_SUCCESS) {
        reinterpret_cast<ResultSet*>(response->cookie)->setRescode(0);
    } else {
        reinterpret_cast<ResultSet*>(response->cookie)->setRescode(Error(Error::SUBSYSf_VIEWS,Error::VIEWS_MISMATCH));
   }
}

}
void
ViewExecutor::runSingleView(lcb_CMDVIEWQUERY *cmd, ResultSet& out)
{
    lcb_error_t lcb_err;

    lcb_err = lcb_view_query(handle->getLcb(),
            &out, cmd);

    if (lcb_err != LCB_SUCCESS) {
        goto GT_ERR;
    }

    lcb_wait3(handle->getLcb(), LCB_WAIT_NOCHECK);

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

    lcb_CMDVIEWQUERY vq = { 0 };
    vq.ddoc = dname.c_str();
    vq.nddoc =  dname.size();
    vq.view = vname.c_str();
    vq.nview = vname.size();
    vq.optstr = optstr.c_str();
    vq.noptstr = optstr.size();
    vq.callback = rowCallback;


    handle->externalEnter();

    while (!handle->isCancelled()) {

        rs->markBegin();
        runSingleView(&vq, out);

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

    handle->externalLeave();
    return true;
}
}
