#ifndef VOLTA_AGENT_PLATFORM_CPU_INFO_H_
#define VOLTA_AGENT_PLATFORM_CPU_INFO_H_

#include <string>

namespace volta {
namespace agent {
namespace platform {

enum class CpuVendor {
    UNKNOWN,
    AMD,
    INTEL
};

struct CpuInfo {
    // Crucial determinant for Collector selection
    CpuVendor vendor = CpuVendor::UNKNOWN;
    
    // Whole model name a.e. "Intel Core i7-9700K"
    std::string model_name;

    // CPU architecture e.g. "x86_64"
    std::string architecture;

    // Topology information
    int physical_cores = 0;
    int logical_threads = 0;
    int socket_count = 1;

    double base_frequency_mhz = 0.0;
    double max_frequency_mhz = 0.0;

    int64_t l1_cache_size_bytes = 0;
    int64_t l2_cache_size_bytes = 0;
    int64_t l3_cache_size_bytes = 0;

    bool has_rapl = false;      // Support Intel RAPL
    bool has_zenpower = false;  // Support AMD zenpower/k10temp
    bool has_pmu = false;       // Access to Performance Monitoring Unit
};

} // namespace platform
} // namespace agent
} // namespace volta

#endif  // VOLTA_AGENT_PLATFORM_CPU_INFO_H_