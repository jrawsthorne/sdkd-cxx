#include "sdkd_internal.h"
#include <couchbase/logger/configuration.hxx>
#include <ctime>

namespace CBSdkd
{
void
create_logger(const std::string& path)
{
    // TODO: File logger requires the client to support custom spdlog sinks because
    // sdkd expects a static filename whereas the defaul client logger uses rotating logs
    // Use a console logger for now
    if (!couchbase::logger::is_initialized()) {

        // append current time to the log file name so tests have a unique log file
        auto now = std::chrono::steady_clock::now();
        auto real_path = fmt::format("{}.{}", path, now.time_since_epoch().count());

        // auto logger = spdlog::basic_logger_mt(LOGGER_NAME, path);
        couchbase::logger::configuration configuration{ real_path };
        configuration.log_level = couchbase::logger::level::trace;
        couchbase::logger::create_file_logger(configuration);
        // couchbase::logger::create_console_logger();
        // couchbase::logger::set_log_levels(couchbase::logger::level::trace);
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