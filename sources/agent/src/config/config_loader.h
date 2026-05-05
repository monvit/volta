#ifndef VOLTA_AGENT_CONFIG_CONFIG_LOADER_H_
#define VOLTA_AGENT_CONFIG_CONFIG_LOADER_H_

#include <filesystem>
#include <map>
#include <set>
#include <toml++/toml.hpp>

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
  ConfigLoader(const ConfigLoader&) = delete;
  ConfigLoader(ConfigLoader&&) = delete;
  void operator=(const ConfigLoader&) = delete;
  void operator=(ConfigLoader&&) = delete;

  static void LoadConfigFile(Config& out_config);
  static void CreateUUID(Config& out_config);
  static void LoadCoreAffinity(toml::table& tbl, Config& out_config);
  static void LoadInterval(toml::table& tbl, Config& out_config);
  static void LoadServerAddress(toml::table& tbl, Config& out_config);
  static void LoadServerPort(toml::table& tbl, Config& out_config);
  static void LoadCollectors(toml::table& tbl, Config& out_config);
  static void CheckKeys(toml::table& tbl);

  static std::filesystem::path kConfigFile;
  static std::filesystem::path kUUIDFile;
  static std::set<std::string_view, std::less<>> kValidTopLevelKeys;
  static std::map<std::string_view, std::set<std::string_view, std::less<>>,
                  std::less<>>
      kValidCollectors;
};

}  // namespace config
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CONFIG_CONFIG_LOADER_H_
