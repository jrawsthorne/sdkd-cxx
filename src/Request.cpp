/*
 * Request.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include "sdkd_internal.h"

namespace CBSdkd {


bool
Request::refreshWith(const string& str, bool reset)
{
    bool ret = Message::refreshWith(str, reset);
    if (ret) {
        payload = payload[CBSDKD_MSGFLD_REQDATA];
        if (!payload) {
            this->err = Error(Error::SUBSYSf_SDKD,
                              Error::SDKD_EINVAL,
                              "Couldn't find CommandData ");
            ret = false;
        }
    }
    return ret;
}

Request::Request(std::string& str): Message(str) {
    refreshWith(str, false);
}

Request::Request() {
}


bool
Request::isValid() const
{
    return this->err == false;
}

Request *
Request::decode(string& str, Error *errp, bool keep_on_error)
{
    Request *ret = new Request(str);
    if (ret->isValid() == false) {
        if (errp) {
            *errp = ret->err;
        }
        if (keep_on_error == false) {
            delete ret;
            return NULL;
        }
    }
    return ret;
}


} /* namespace CBSdkd */
