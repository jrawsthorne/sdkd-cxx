#include "sdkd_internal.h"

namespace CBSdkd
{
void
create_logger(const std::string& path)
{
    if (!couchbase::logger::isInitialized()) {
        auto logger = spdlog::basic_logger_mt(LOGGER_NAME, path);
        couchbase::logger::register_spdlog_logger(logger);
        couchbase::logger::set_log_levels(spdlog::level::trace);
    }
}

void
destroy_logger()
{
    if (!couchbase::logger::isInitialized()) {
        couchbase::logger::unregister_spdlog_logger(LOGGER_NAME);
    }
}
} // namespace CBSdkd