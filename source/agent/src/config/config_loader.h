#ifndef VOLTA_AGENT_CONFIG_CONFIG_LOADER_H_
#define VOLTA_AGENT_CONFIG_CONFIG_LOADER_H_

#include "agent/src/config/config.h"
#include <string>
#include <filesystem>

namespace volta {
namespace agent {
namespace config {
    
class ConfigLoader {
public:
    static Config LoadConfig();
    static Config LoadConfig(const std::filesystem::path& filepath);

private:
};

} // namespace config
} // namespace agent
} // namespace volta

#endif  // VOLTA_AGENT_CONFIG_CONFIG_LOADER_H_
