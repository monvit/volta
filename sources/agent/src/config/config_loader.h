#ifndef VOLTA_AGENT_CONFIG_CONFIG_LOADER_H_
#define VOLTA_AGENT_CONFIG_CONFIG_LOADER_H_

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <toml++/toml.hpp>

#include "config/config.h"

namespace volta {
namespace agent {
namespace config {

class ConfigLoader {
 public:
  static Config LoadConfig(const std::optional<std::filesystem::path>& file);
  static Config LoadDefaultConfig();

 private:
  ConfigLoader() = delete;
  ConfigLoader(const ConfigLoader&) = delete;
  ConfigLoader(ConfigLoader&&) = delete;
  void operator=(const ConfigLoader&) = delete;
  void operator=(ConfigLoader&&) = delete;

  static void LoadConfigFile(const std::filesystem::path& path,
                             Config& out_config);
  static void CreateUUID(Config& out_config);
  static void LoadCoreAffinity(toml::table& tbl, Config& out_config);
  static void LoadInterval(toml::table& tbl, Config& out_config);
  static void LoadServerAddress(toml::table& tbl, Config& out_config);
  static void LoadServerPort(toml::table& tbl, Config& out_config);
  static void LoadMetrics(toml::table& tbl, Config& out_config);
  static void LoadTimeWindow(toml::table& tbl, Config& out_config);
  static void CheckKeys(toml::table& tbl);

  static std::filesystem::path kUUIDFile;
  static std::set<std::string_view, std::less<>> kValidTopLevelKeys;
};

}  // namespace config
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CONFIG_CONFIG_LOADER_H_
