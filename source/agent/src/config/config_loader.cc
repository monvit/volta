#include "config/config_loader.h"

namespace volta {
namespace agent {
namespace config {

Config ConfigLoader::LoadConfig() {
    Config config;

    CollectorConfig nvml_collector;
    nvml_collector.enabled = true;
    nvml_collector.metrics = {
        {"gpu_utilization", true},
        {"memory_utilization", true},
        {"temperature", true},
    };
    config.collectors[CollectorNames::kNvml] = nvml_collector;

    CollectorConfig proc_stat_config;
    proc_stat_config.enabled = true;
    proc_stat_config.metrics["cpu_usage_percent"] = true;
    config.collectors[CollectorNames::kProcStat] = proc_stat_config;

    return config;
}

Config ConfigLoader::LoadConfig(const std::filesystem::path& filepath) {
    // ignore for POC
    return LoadConfig();
}

} // namespace config
} // namespace agent
} // namespace volta
