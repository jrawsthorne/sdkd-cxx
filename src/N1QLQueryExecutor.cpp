#include "sdkd_internal.h"
namespace CBSdkd {

N1QLQueryExecutor::N1QLQueryExecutor(Handle *handle) {
    this->handle = handle;
}

N1QLQueryExecutor::~N1QLQueryExecutor() {

}

bool
N1QLQueryExecutor::execute(Command cmd,
                          ResultSet& out,
                          const ResultOptions& options,
                          const Request& req) {

    return true;
}
}


