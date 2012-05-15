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
    Request();

    virtual ~Request() {
        // TODO Auto-generated destructor stub
    }
    bool isValid();
    virtual bool refreshWith(const string&, bool);

    static Request* decode(std::string&, Error *errp,
                           bool keep_on_error = false);

};

} /* namespace CBSdkd */
#endif /* REQUEST_H_ */
