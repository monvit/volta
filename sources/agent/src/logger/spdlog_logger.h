#ifndef VOLTA_AGENT_SRC_SPD_LOGGER_H
#define VOLTA_AGENT_SRC_SPD_LOGGER_H
#include <spdlog/spdlog.h>

#include <memory>

#include "logger.h"
namespace volta::agent::logging {

class SpdlogLogger final : public Logger {
 public:
  explicit SpdlogLogger(std::shared_ptr<spdlog::logger> impl)
      : impl_(std::move(impl)) {
    impl_->set_pattern("[%^%l%$] [%n] %v");
  }

  explicit SpdlogLogger(std::shared_ptr<spdlog::logger> impl,
                        std::string_view name)
      : impl_(std::move(impl->clone(std::string(name)))) {
    impl_->set_pattern("[%^%l%$] [%n] %v");
  }

  std::unique_ptr<Logger> WithName(std::string_view name) const override {
    return std::make_unique<SpdlogLogger>(impl_->clone(std::string(name)));
  }

  bool ShouldLog(LogLevel l) const override {
    return impl_->should_log(ToSpdlog(l));
  }

  void SetLevel(LogLevel level) override { impl_->set_level(ToSpdlog(level)); }

  LogLevel GetLevel() const override { return ToLogLevel(impl_->level()); }

 private:
  void Write(LogLevel l, std::string_view msg) override {
    impl_->log(ToSpdlog(l), msg);
  }

  static inline spdlog::level::level_enum ToSpdlog(LogLevel l) {
    switch (l) {
      case LogLevel::NOISY_LEVEL:
        return spdlog::level::trace;
      case LogLevel::DEBUG_LEVEL:
        return spdlog::level::debug;
      case LogLevel::INFO_LEVEL:
        return spdlog::level::info;
      case LogLevel::WARNING_LEVEL:
        return spdlog::level::warn;
      case LogLevel::ERROR_LEVEL:
        return spdlog::level::err;
      default:
        return spdlog::level::info;
    }
  }

  static inline LogLevel ToLogLevel(spdlog::level::level_enum l) {
    switch (l) {
      case spdlog::level::trace:
        return LogLevel::NOISY_LEVEL;
      case spdlog::level::debug:
        return LogLevel::DEBUG_LEVEL;
      case spdlog::level::info:
        return LogLevel::INFO_LEVEL;
      case spdlog::level::warn:
        return LogLevel::WARNING_LEVEL;
      case spdlog::level::err:
        return LogLevel::ERROR_LEVEL;
      default:
        return LogLevel::INFO_LEVEL;
    }
  }

  std::shared_ptr<spdlog::logger> impl_;
};

}  // namespace volta::agent::logging
#endif
