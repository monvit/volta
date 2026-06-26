#ifndef VOLTA_AGENT_PLATFORM_CPU_INFO_H_
#define VOLTA_AGENT_PLATFORM_CPU_INFO_H_

#include <cstdint>
#include <string>

namespace volta {
namespace agent {
namespace platform {

enum class CpuVendor { UNKNOWN, AMD, INTEL };

struct CpuInfo {
  CpuVendor vendor = CpuVendor::UNKNOWN;

  std::string model_name;
  std::string architecture;

  // Topology information
  uint8_t physical_cores = 0;
  uint8_t logical_threads = 0;
  uint8_t socket_count = 1;

  float base_frequency_mhz = 0.0;
  float max_frequency_mhz = 0.0;

  uint64_t l1_cache_size_bytes = 0;
  uint64_t l2_cache_size_bytes = 0;
  uint64_t l3_cache_size_bytes = 0;

  float power_limit_watts = 0.0;

  bool has_rapl = false;      // Support Intel RAPL
  bool has_zenpower = false;  // Support AMD zenpower/k10temp
  bool has_pmu = false;       // Access to Performance Monitoring Unit
};

}  // namespace platform
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_PLATFORM_CPU_INFO_H_
