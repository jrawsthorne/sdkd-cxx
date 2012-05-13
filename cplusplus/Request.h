/*
 * Request.h
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#ifndef REQUEST_H_
#define REQUEST_H_

#include "Message.h"
#include "Error.h"

namespace CBSdkd {

class Request : public CBSdkd::Message {
public:
    Request(std::string&);

    virtual ~Request() {
        // TODO Auto-generated destructor stub
    }
    bool isValid();

    static Request* decode(std::string&, Error *errp);

};

} /* namespace CBSdkd */
#endif /* REQUEST_H_ */
