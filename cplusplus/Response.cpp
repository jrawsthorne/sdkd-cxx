/*
 * Response.cpp
 *
 *  Created on: May 12, 2012
 *      Author: mnunberg
 */

#include "/home/mnunberg/src/cbsdkd/cplusplus/Response.h"

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

    root["Command"] = this->command.cmdstr;
    root["ReqID"] = (int)this->reqid;
    root["Handle"] = (int)this->handle_id;

    if (!this->response_data) {
        cerr << "No response data for command...\n";
    } else {
        root["ResponseData"] = this->response_data;
    }

    if (this->err) {
        root["Status"] = this->err.code;
        root["ErrorString"] = this->err.errstr;
    } else {
        root["Status"] = 0;
    }

    return Json::FastWriter().write(root);
}

} /* namespace CBSdkd */
