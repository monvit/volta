#ifndef VOLTA_AGENT_CONFIG_CONFIG_LOADER_H_
#define VOLTA_AGENT_CONFIG_CONFIG_LOADER_H_

#include <filesystem>
#include <map>
#include <set>

#include "config/config.h"

namespace volta {
namespace agent {
namespace config {

class ConfigLoader {
  public:
    static Config LoadConfig();
    static Config LoadDefaultConfig();

  private:
    ConfigLoader() = delete;

    static void LoadConfigFile(Config &out_config);

    static std::filesystem::path kConfigFile;
    static std::set<std::string> kValidTopLevelKeys;
    static std::map<std::string, std::set<std::string>> kValidCollectorMetrics;
};

} // namespace config
} // namespace agent
} // namespace volta

#endif // VOLTA_AGENT_CONFIG_CONFIG_LOADER_H_
