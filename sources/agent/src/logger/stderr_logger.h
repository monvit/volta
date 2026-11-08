#ifndef VOLTA_AGENT_SRC_STDERR_LOGGER_H
#define VOLTA_AGENT_SRC_STDERR_LOGGER_H

#include <iostream>

#include "logger/logger.h"

namespace volta::agent::logging {

class StderrLogger final : public Logger {
 public:
  ~StderrLogger() {};
  std::unique_ptr<Logger> WithName(std::string_view name) const {
    return std::make_unique<StderrLogger>();
  }

  bool ShouldLog(LogLevel level) const { return current_level_ <= level; }
  void SetLevel(LogLevel level) { current_level_ = level; }
  LogLevel GetLevel() const { return current_level_; }

 private:
  LogLevel current_level_ = LogLevel::NOISY_LEVEL;

 protected:
  void Write(LogLevel level, std::string_view message) { std::cerr << message; }
};
}  // namespace volta::agent::logging
#endif
