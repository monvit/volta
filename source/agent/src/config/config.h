#ifndef VOLTA_AGENT_CONFIG_CONFIG_H_
#define VOLTA_AGENT_CONFIG_CONFIG_H_

#include <map>
#include <string>
#include <chrono>

namespace volta {
namespace agent {
namespace config {

struct CollectorConfig {
    bool enabled = false;                   // Is collector even enabled
    std::map<std::string, bool> metrics;    // Specific metrics to collect
};

struct Config {
    std::chrono::milliseconds collection_interval = std::chrono::milliseconds(2000);
    int32_t core_affinity = -1;                  // No specific core affinity
    
    std::string server_address = "localhost";
    u_int16_t server_port = 50051;

    std::map<std::string, CollectorConfig> collectors;
};

} // namespace config
} // namespace agent
} // namespace volta

#endif  // VOLTA_AGENT_CONFIG_CONFIG_H_
