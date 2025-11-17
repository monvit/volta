#include "agent/src/config/config_loader.h"

namespace volta {
namespace agent {
namespace config {

Config ConfigLoader::LoadConfig() {
    Config config;

    config.collection_interval = std::chrono::milliseconds(2000);
    config.core_affinity = -1; // No specific core affinity
    config.server_address = "localhost";
    config.server_port = 50051;

    // Enable NVML collector
    CollectorConfig nvml_collector;
    nvml_collector.enabled = true;
    nvml_collector.metrics = {
        {"gpu_utilization", true},
        {"memory_utilization", true},
        {"temperature", true},
    };
    config.collectors["nvml"] = nvml_collector;

    // Enable CPU collector
    CollectorConfig proc_stat_config;
    proc_stat_config.enabled = true;
    proc_stat_config.metrics["cpu_usage_percent"] = true;
    config.collectors["proc_stat"] = proc_stat_config;

    return config;
}

Config ConfigLoader::LoadConfig(const std::filesystem::path& filepath) {
    return LoadConfig();
}

} // namespace config
} // namespace agent
} // namespace volta
