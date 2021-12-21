#include "sdkd_internal.h"

namespace CBSdkd
{
void
create_logger(const std::string& path)
{
    // TODO: File logger requires the client to support custom spdlog sinks because
    // sdkd expects a static filename whereas the defaul client logger uses rotating logs
    // Use a console logger for now
    if (!couchbase::logger::is_initialized()) {
        // auto logger = spdlog::basic_logger_mt(LOGGER_NAME, path);
        couchbase::logger::create_console_logger();
    }
}

void
destroy_logger()
{
    if (couchbase::logger::is_initialized()) {
        couchbase::logger::shutdown();
    }
}
} // namespace CBSdkd