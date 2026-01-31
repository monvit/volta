#ifndef VOLTA_AGENT_CONFIG_CONFIG_H_
#define VOLTA_AGENT_CONFIG_CONFIG_H_

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
// TODO: Metrics and Collector names into something that can be read with a string
namespace CollectorNames {
// CPU
static constexpr char const *kProcStat = "proc_stat";
static constexpr char const *kCpuFreq = "cpu_freq";
static constexpr char const *kRapl = "rapl";
static constexpr char const *kZenPower = "zenpower";
static constexpr char const *kPmu = "pmu";

// GPU
static constexpr char const *kNvml = "nvml";
static constexpr char const *kDcgm = "dcgm";
static constexpr char const *kRocm = "rocm";
static constexpr char const *kLevelZero = "level_zero";

// RAM
static constexpr char const *kMemInfo = "mem_info";
static constexpr char const *kVmStat = "vm_stat";

// Disc and Network (I/O)
static constexpr char const *kDiskStats = "disk_stats";
static constexpr char const *kNetDev = "net_dev";
} // namespace CollectorNames

struct CollectorConfig {
    bool enabled = false;
    std::map<std::string, bool> metrics;
};

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
            if (CPU_ISSET(i, &set))
                std::cout << i << " ";
        }
        std::cout << "\n";
    }

    static constexpr int32_t kDefaultIntervalMs = 500;
    static constexpr char const *kDefaultServerAddress = "localhost";
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

    std::chrono::milliseconds collection_interval = std::chrono::milliseconds(kDefaultIntervalMs);
    cpu_set_t core_affinity = kDefaultAffinity;

    std::string server_address = kDefaultServerAddress;
    uint16_t server_port = kDefaultServerPort;

    std::map<std::string, CollectorConfig> collectors;
};

} // namespace config
} // namespace agent
} // namespace volta

#endif // VOLTA_AGENT_CONFIG_CONFIG_H_
