#ifndef LOGGER_H
#define LOGGER_H
#endif

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#include <core/logger/logger.hxx>
#include "spdlog/sinks/basic_file_sink.h"

namespace CBSdkd
{
const std::string LOGGER_NAME = "sdkd_logger";
void
create_logger(const std::string& path);
void
destroy_logger();
} // namespace CBSdkd