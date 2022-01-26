#ifndef SDKD_SUBDOC_H_
#define SDKD_SUBDOC_H_
#endif

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#include "sdkd_internal.h"

namespace CBSdkd {
using namespace std;

class SDLoader : protected DebugContext {

public:
    SDLoader(Handle* handle) {
        this->handle = handle;
    }
    virtual ~SDLoader() {}

    bool populate(const Dataset& ds);

private:
    Handle* handle;
};
}


