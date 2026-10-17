#include "logger/logger_builder.h"

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <string>

#include "logger/spdlog_logger.h"

namespace volta::agent::logging {

std::unique_ptr<Logger> BuildLogger(const config::Config& config,
                                    std::string_view name) {
  const std::string logger_name(name);
  std::shared_ptr<spdlog::sinks::sink> sink;
  if (config.log_file) {
    sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        config.log_file->string(), false);
  } else {
    sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
  }

  auto backend = std::make_shared<spdlog::logger>(logger_name, sink);
  auto logger = std::make_unique<SpdlogLogger>(backend);
  logger->SetLevel(config.log_level);
  return logger;
}

}  // namespace volta::agent::logging
