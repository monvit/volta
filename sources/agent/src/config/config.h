#ifndef VOLTA_AGENT_CONFIG_CONFIG_H_
#define VOLTA_AGENT_CONFIG_CONFIG_H_

#include <metric.h>
#include <sched.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <thread>

namespace volta {
namespace agent {
namespace config {

struct Config {
  void PrintCurrentAffinity() {
    cpu_set_t set;
    CPU_ZERO(&set);

    if (sched_getaffinity(0, sizeof(set), &set) != 0) {
      // TODO: Log
      perror("sched_getaffinity");
      return;
    }

    long max_cpus = sysconf(_SC_NPROCESSORS_CONF);
    std::cout << "Current CPU affinity: ";

    for (int i = 0; i < max_cpus; ++i) {
      if (CPU_ISSET(i, &set)) std::cout << i << " ";
    }
    std::cout << "\n";
  }
  // TODO: move this initialization logic to the ConfigLoader's
  // LoadDefaultConfig method
  static constexpr int32_t kDefaultIntervalMs = 500;
  static constexpr int32_t kDefaultTimeWindowMs = 2000;
  static constexpr char const* kDefaultServerAddress = "localhost";
  static constexpr uint16_t kDefaultServerPort = 50051;
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

  std::string uuid = "";

  std::chrono::milliseconds collection_interval =
      std::chrono::milliseconds(kDefaultIntervalMs);
  cpu_set_t core_affinity = kDefaultAffinity;
  std::string server_address = kDefaultServerAddress;
  uint16_t server_port = kDefaultServerPort;
  std::vector<MetricType> requestedMetrics;
  std::chrono::milliseconds buffered_time_window =
      std::chrono::milliseconds(kDefaultTimeWindowMs);
};

}  // namespace config
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CONFIG_CONFIG_H_
