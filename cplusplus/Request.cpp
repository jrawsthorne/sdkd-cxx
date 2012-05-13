/*
 * Request.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include "/home/mnunberg/src/cbsdkd/cplusplus/Request.h"

namespace CBSdkd {


Request::Request(std::string& str): Message(str) {
    this->payload = this->payload["CommandData"];
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
Request::decode(string& str, Error *errp)
{
    Request *ret = new Request(str);
    if (ret->isValid() == false) {
        *errp = ret->err;
        delete ret;
        return NULL;
    }
    return ret;
}


} /* namespace CBSdkd */
