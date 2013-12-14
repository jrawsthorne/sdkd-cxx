#include "sdkd_internal.h"
#include <cassert>
#include <iostream>

using namespace CBSdkd;
DebugContext CBSdkd::CBsdkd_Global_Debug_Context;
Daemon* Daemon::MainDaemon = NULL;

int main(void) {
    DatasetSeedSpecification spec;
    spec.continuous = false;
    spec.count = 10000;
    spec.kseed = "SimpleKey";
    spec.vseed = "SimpleValue";
    spec.ksize = 32;
    spec.vsize = 128;
    spec.repeat = "_REP_";

    DatasetSeededIterator iter(&spec);
    iter.start();

    for (int i = 0; i < spec.count; i++, iter.advance()) {
        std::string key = iter.key();
        std::string val = iter.value();
        assert(key.length() < 50);
        assert(val.length() < 200);
        std::cout << "Key: " << key << ", Val: " << val << std::endl;
    }

    return 0;
}
