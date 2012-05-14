/*
 * Message.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include "Message.h"
#include "cbsdkd.h"

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
        err = Error(Error::SUBSYSf_SDKD, Error::SDKD_EINVAL,
                    reader.getFormatedErrorMessages());
        return;
    }

    this->reqid = json[CBSDKD_MSGFLD_REQID].asUInt();
    this->handle_id = json[CBSDKD_MSGFLD_HID].asUInt();
    this->command = Command(json[CBSDKD_MSGFLD_CMD].asString());
    this->payload = json;
}

Message::~Message() {
    // TODO Auto-generated destructor stub
}

} /* namespace CBSdkd */
