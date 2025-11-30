#ifndef VOLTA_AGENT_CONFIG_CONFIG_H_
#define VOLTA_AGENT_CONFIG_CONFIG_H_

#include <map>
#include <string>
#include <chrono>
#include <cstdint>

namespace volta {
namespace agent {
namespace config {

namespace CollectorNames {
    // CPU
    static constexpr char const* kProcStat = "proc_stat";
    static constexpr char const* kCpuFreq = "cpu_freq";
    static constexpr char const* kRapl = "rapl";
    static constexpr char const* kZenPower = "zenpower";
    static constexpr char const* kPmu = "pmu";

    // GPU
    static constexpr char const* kNvml = "nvml";
    static constexpr char const* kDcgm = "dcgm";
    static constexpr char const* kRocm = "rocm";
    static constexpr char const* kLevelZero = "level_zero";
    
    // RAM
    static constexpr char const* kMemInfo = "mem_info";
    static constexpr char const* kVmStat = "vm_stat";

    // Disc and Network (I/O)
    static constexpr char const* kDiskStats = "disk_stats";
    static constexpr char const* kNetDev = "net_dev";
}

struct CollectorConfig {
    bool enabled = false;
    std::map<std::string, bool> metrics;
};

struct Config {
    static constexpr int32_t kDefaultIntervalMs = 2000;
    static constexpr int32_t kDefaultAffinity = -1;
    static constexpr char const* kDefaultServerAddress = "localhost";
    static constexpr uint16_t kDefaultServerPort = 50051;

    std::chrono::milliseconds collection_interval = std::chrono::milliseconds(kDefaultIntervalMs);
    int32_t core_affinity = kDefaultAffinity;
    
    std::string server_address = kDefaultServerAddress;
    uint16_t server_port = kDefaultServerPort;

    std::map<std::string, CollectorConfig> collectors;
};

} // namespace config
} // namespace agent
} // namespace volta

#endif  // VOLTA_AGENT_CONFIG_CONFIG_H_
