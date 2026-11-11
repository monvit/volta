#include "config/config_resolution.h"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace volta {
namespace agent {
namespace config {
namespace {

void RequireReadable(const std::filesystem::path& path) {
  if (path.empty()) {
    throw ResolutionError("config path is empty", EINVAL);
  }

  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    if (ec) {
      throw ResolutionError(
          "cannot stat config " + path.string() + ": " + ec.message(),
          static_cast<int>(ec.value()));
    }
    throw ResolutionError("config file not found: " + path.string(), ENOENT);
  }

  std::ifstream in(path);
  if (!in) {
    throw ResolutionError("cannot read config " + path.string(), errno);
  }
}

}  // namespace

const char* ConfigSourceToString(ConfigSource source) {
  switch (source) {
    case ConfigSource::kCli:
      return "cli";
    case ConfigSource::kEnvironment:
      return "environment";
    case ConfigSource::kSystem:
      return "system";
    case ConfigSource::kBuiltinDefaults:
      return "builtin-defaults";
  }
  return "unknown";
}

void PrintAgentHelp(const char* prog) {
  std::cout << "Usage: " << prog << " [--config PATH] [--help]\n"
            << "\n"
            << "Volta monitoring agent (voltad).\n"
            << "Config search order: --config >  $" << kConfigEnvVar << " > "
            << kSystemConfigPath << " > built-in defaults.\n";
}

std::optional<CliOptions> ParseCli(int argc, const char* const* argv) {
  CliOptions opts;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      opts.show_help = true;
      continue;
    }
    if (arg == "--config") {
      if (i + 1 >= argc) {
        return std::nullopt;
      }
      opts.config_flag = std::filesystem::path(argv[++i]);
      continue;
    }
    if (arg.starts_with("--config=")) {
      const std::string path = arg.substr(std::string("--config=").size());
      if (path.empty()) {
        return std::nullopt;
      }
      opts.config_flag = std::filesystem::path(path);
      continue;
    }
    return std::nullopt;
  }
  return opts;
}

ResolvedConfig ResolveConfig(const CliOptions& cli,
                             const std::filesystem::path& system_path) {
  if (cli.config_flag) {
    RequireReadable(*cli.config_flag);
    return ResolvedConfig{*cli.config_flag, ConfigSource::kCli};
  }

  if (const char* env_val = std::getenv(kConfigEnvVar)) {
    if (*env_val == '\0') {
      throw ResolutionError(std::string(kConfigEnvVar) + " is set but empty",
                            EINVAL);
    }
    const std::filesystem::path path(env_val);
    RequireReadable(path);
    return ResolvedConfig{path, ConfigSource::kEnvironment};
  }

  std::error_code ec;
  const bool present = std::filesystem::exists(system_path, ec);
  if (ec) {
    throw ResolutionError("cannot stat system config " + system_path.string() +
                              ": " + ec.message(),
                          static_cast<int>(ec.value()));
  }
  if (!present) {
    return ResolvedConfig{std::nullopt, ConfigSource::kBuiltinDefaults};
  }

  RequireReadable(system_path);
  return ResolvedConfig{system_path, ConfigSource::kSystem};
}

}  // namespace config
}  // namespace agent
}  // namespace volta
