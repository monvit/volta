#ifndef VOLTA_AGENT_CONFIG_CONFIG_RESOLUTION_H_
#define VOLTA_AGENT_CONFIG_CONFIG_RESOLUTION_H_

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

namespace volta {
namespace agent {
namespace config {

#ifndef VOLTA_SYSTEM_CONFIG_PATH
#define VOLTA_SYSTEM_CONFIG_PATH "/etc/volta/agent.conf"
#endif

inline constexpr const char* kSystemConfigPath = VOLTA_SYSTEM_CONFIG_PATH;
inline constexpr const char* kConfigEnvVar = "VOLTA_CONFIG";

struct ResolutionError : std::runtime_error {
  int errno_code = 0;

  ResolutionError(const std::string& message, int errno_code = 0)
      : std::runtime_error(message), errno_code(errno_code) {}
};

enum class ConfigSource {
  kCli,
  kEnvironment,
  kSystem,
  kBuiltinDefaults,
};

const char* ConfigSourceToString(ConfigSource source);

struct ResolvedConfig {
  std::optional<std::filesystem::path> path;
  ConfigSource source = ConfigSource::kBuiltinDefaults;
};

struct CliOptions {
  std::optional<std::filesystem::path> config_flag;
  bool show_help = false;
};

std::optional<CliOptions> ParseCli(int argc, const char* const* argv);
void PrintAgentHelp(const char* prog);

ResolvedConfig ResolveConfig(
    const CliOptions& cli,
    const std::filesystem::path& system_path = kSystemConfigPath);

}  // namespace config
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CONFIG_CONFIG_RESOLUTION_H_
