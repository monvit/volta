#ifndef VOLTA_AGENT_CONFIG_CONFIG_H_
#define VOLTA_AGENT_CONFIG_CONFIG_H_

#include <metric.h>
#include <sched.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#include "logger/logger.h"

namespace volta {
namespace agent {
namespace config {

struct Config {
  void PrintCurrentAffinity(logging::Logger& logger) {
    cpu_set_t set;
    CPU_ZERO(&set);

    if (sched_getaffinity(0, sizeof(set), &set) != 0) {
      logger.Log(logging::LogLevel::ERROR_LEVEL, "sched_getaffinity failed: {}",
                 std::strerror(errno));
      return;
    }

    long max_cpus = sysconf(_SC_NPROCESSORS_CONF);
    std::ostringstream affinity;
    for (int i = 0; i < max_cpus; ++i) {
      if (CPU_ISSET(i, &set)) affinity << i << " ";
    }
    logger.Log(logging::LogLevel::INFO_LEVEL, "Current CPU affinity: {}",
               affinity.str());
  }
  // TODO: move this initialization logic to the ConfigLoader's
  // LoadDefaultConfig method
  static constexpr int32_t kDefaultIntervalMs = 500;
  static constexpr int32_t kDefaultTimeWindowMs = 2000;
  static constexpr char const* kDefaultServerAddress = "localhost";
  static constexpr const char* kDefaultDumpPath = "/var/lib/volta/dumps";
  static constexpr uint16_t kDefaultServerPort = 5000;
  static inline cpu_set_t kDefaultAffinity = [] {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    long n_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    n_cpus = std::min(n_cpus, static_cast<long>(CPU_SETSIZE));
    // TODO: Handle sysconf error
    for (long i = 0; i < n_cpus; ++i) {
      CPU_SET(i, &mask);
    }
    return mask;
  }();

  std::chrono::milliseconds collection_interval =
      std::chrono::milliseconds(kDefaultIntervalMs);
  cpu_set_t core_affinity = kDefaultAffinity;
  std::string server_address = kDefaultServerAddress;
  uint16_t server_port = kDefaultServerPort;
  std::vector<MetricType> requestedMetrics;
  std::chrono::milliseconds buffered_time_window =
      std::chrono::milliseconds(kDefaultTimeWindowMs);
  std::filesystem::path dump_dir = kDefaultDumpPath;
  logging::LogLevel log_level = logging::LogLevel::INFO_LEVEL;
  std::optional<std::filesystem::path> log_file;
};

}  // namespace config
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CONFIG_CONFIG_H_
