/*
 * Message.h
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_
#include <json/json.h>
#include "Error.h"

#define CBSDKD_XCOMMAND(XX) \
    XX(MC_DS_MUTATE_SET) \
    XX(MC_DS_MUTATE_APPEND) \
    XX(MC_DS_MUTATE_PREPEND) \
    XX(MC_DS_MUTATE_ADD) \
    XX(MC_DS_MUTATE_REPLACE) \
    XX(MC_DS_DELETE) \
    XX(MC_DS_TOUCH) \
    XX(MC_DS_GET) \
    XX(NEWHANDLE) \
    XX(NEWDATASET)

namespace CBSdkd {

using namespace std;

class Command {

public:
    std::string cmdstr;

    enum Code {

#define X(c) c,
        CBSDKD_XCOMMAND(X)
#undef X
        INVALID_COMMAND
    };

    Code code;

    Command() { };
    Command(const std::string&);
    Command(Code);

    ~Command() { };


    operator int () { return this->code; };
};

class Message {

public:
    Message(std::string& str);
    Message() { };
    virtual ~Message();

    unsigned long reqid;
    unsigned long handle_id;
    Command command;

    Json::Value payload;

protected:
    Error err;
};

} /* namespace CBSdkd */
#endif /* MESSAGE_H_ */
