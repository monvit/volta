#ifndef VOLTA_AGENT_SRC_LOGGER_H
#define VOLTA_AGENT_SRC_LOGGER_H

#include <format>
#include <memory>
#include <string_view>
#include <utility>

namespace volta::agent::logging {

enum class LogLevel {
  NOISY_LEVEL,
  DEBUG_LEVEL,
  INFO_LEVEL,
  WARNING_LEVEL,
  ERROR_LEVEL
};

class Logger {
 public:
  virtual ~Logger() = default;
  virtual std::unique_ptr<Logger> WithName(std::string_view name) const = 0;

  virtual bool ShouldLog(LogLevel level) const = 0;
  virtual void SetLevel(LogLevel level) = 0;
  virtual LogLevel GetLevel() const = 0;

  template <typename... Args>
  void Log(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
    if (!ShouldLog(level)) return;
    Write(level, std::format(fmt, std::forward<Args>(args)...));
  }

 protected:
  virtual void Write(LogLevel level, std::string_view message) = 0;
};

}  // namespace volta::agent::logging
#endif
