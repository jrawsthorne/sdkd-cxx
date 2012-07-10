/*
 * Response.cpp
 *
 *  Created on: May 12, 2012
 *      Author: mnunberg
 */

#include "Response.h"
#include "cbsdkd.h"
#include "contrib/debug++.h"

namespace CBSdkd {

Response::Response() {

}

Response::Response(const Request* req, Error const& err)
: Message::Message(*req)
{
    if (err) {
        this->err = err;
    }
}

const string
Response::encode() const {
    Json::Value root;

    root[CBSDKD_MSGFLD_CMD] = this->command.cmdstr;
    root[CBSDKD_MSGFLD_REQID] = (int)this->reqid;
    root[CBSDKD_MSGFLD_HID] = (int)this->handle_id;

    if (!this->response_data) {
        if (this->command.code != Command::NEWHANDLE &&
                this->command.code != Command::CANCEL) {
            log_noctx_warn("No response data for command..");
        }
        root[CBSDKD_MSGFLD_RESDATA] = Json::Value(Json::objectValue);
    } else {
        root[CBSDKD_MSGFLD_RESDATA] = this->response_data;
    }

    if (this->err) {
        root[CBSDKD_MSGFLD_STATUS] = this->err.code;
        root[CBSDKD_MSGFLD_ERRSTR] = this->err.errstr;
    } else {
        root[CBSDKD_MSGFLD_STATUS] = 0;
    }

    return Json::FastWriter().write(root);
}

void
Response::setResponseData(Json::Value& rdata)
{
    response_data = rdata;
}

} /* namespace CBSdkd */
