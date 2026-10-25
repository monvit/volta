#ifndef VOLTA_AGENT_SRC_LOGGER_LOGGER_BUILDER_H_
#define VOLTA_AGENT_SRC_LOGGER_LOGGER_BUILDER_H_

#include <memory>
#include <string_view>

#include "config/config.h"
#include "logger/logger.h"

namespace volta::agent::logging {

std::unique_ptr<Logger> BuildLogger(const config::Config& config,
                                    std::string_view name = "Agent");

}  // namespace volta::agent::logging

#endif  // VOLTA_AGENT_SRC_LOGGER_LOGGER_BUILDER_H_
