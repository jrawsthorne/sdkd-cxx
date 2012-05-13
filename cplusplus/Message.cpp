/*
 * Message.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include "Message.h"
#include <cstdlib>

namespace CBSdkd {

Command::Command(Command::Code code)
{
    this->code = code;
#define X(c) if (code == c) this->cmdstr = #c;
    CBSDKD_XCOMMAND(X)
#undef X

}

Command::Command(const string& str)
{
    // Figure out what our command is..
#define X(c) if (str == #c) this->code = c;
    CBSDKD_XCOMMAND(X)
#undef X

    if (!this->code) {
        cerr << "Couldn't determine code from " << str;
        abort();
    }

    this->cmdstr = str;
}

Message::Message(string& str) {
    Json::Value json;
    Json::Reader reader;

    if (! reader.parse(str, json, false)) {
        cerr << "Couldn't parse JSON string "
                << str << ":"
                << reader.getFormatedErrorMessages();

        abort();
    }

    this->reqid = json["ReqID"].asUInt();
    this->handle_id = json["Handle"].asUInt();
    this->command = Command(json["Command"].asString());
    this->payload = json;
}

Message::~Message() {
    // TODO Auto-generated destructor stub
}

} /* namespace CBSdkd */
