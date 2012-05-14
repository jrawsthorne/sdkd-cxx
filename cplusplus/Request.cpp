/*
 * Request.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include "Request.h"
#include "cbsdkd.h"

namespace CBSdkd {


Request::Request(std::string& str): Message(str) {
    this->payload = this->payload[CBSDKD_MSGFLD_REQDATA];
    if (!this->payload) {
        this->err = Error(Error::SUBSYSf_SDKD,
                          Error::SDKD_EINVAL,
                          "Couldn't find CommandData ");
    }
}


bool
Request::isValid()
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
