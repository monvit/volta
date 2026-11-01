#include "control/control_socket_path.h"

#include <cstdlib>
#include <optional>
#include <system_error>

namespace volta {
namespace agent {
namespace control {
namespace {

constexpr char kMissingRuntimeMessage[] =
    "no control socket path: set $VOLTA_CONTROL_SOCKET, $RUNTIME_DIRECTORY, "
    "or $XDG_RUNTIME_DIR";

std::optional<std::string> Env(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') return std::nullopt;
  return std::string(value);
}

bool IsDir(const std::filesystem::path& path) {
  std::error_code ec;
  return std::filesystem::is_directory(path, ec);
}

std::filesystem::path RequireAbsolute(const std::string& value) {
  std::filesystem::path path(value);
  if (!path.is_absolute()) {
    throw ControlPathError("$VOLTA_CONTROL_SOCKET must be an absolute path");
  }
  return path;
}

void MakeParent(const std::filesystem::path& socket) {
  auto parent = socket.parent_path();
  if (parent.empty() || IsDir(parent)) return;
  std::error_code ec;
  if (!std::filesystem::create_directory(parent, ec)) {
    throw ControlPathError("cannot create " + parent.string());
  }
}

std::filesystem::path XdgSocket(const std::string& xdg) {
  return std::filesystem::path(xdg) / "volta" / "control.sock";
}

void CreateXdgDir(const std::string& xdg) {
  auto dir = std::filesystem::path(xdg) / "volta";
  if (IsDir(dir)) return;
  std::error_code ec;
  if (!std::filesystem::create_directory(dir, ec)) {
    throw ControlPathError("cannot create " + dir.string());
  }
  std::filesystem::permissions(dir, std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace, ec);
  if (ec) throw ControlPathError("cannot create " + dir.string());
}

std::filesystem::path ResolveDaemon() {
  if (auto override_path = Env(kControlSocketEnv)) {
    auto path = RequireAbsolute(*override_path);
    MakeParent(path);
    return path;
  }
  if (auto runtime = Env(kRuntimeDirectoryEnv)) {
    if (!IsDir(*runtime)) {
      throw ControlPathError(
          "control socket runtime directory was not provided "
          "($RUNTIME_DIRECTORY)");
    }
    return std::filesystem::path(*runtime) / "control.sock";
  }
  if (auto xdg = Env(kXdgRuntimeDirEnv)) {
    CreateXdgDir(*xdg);
    return XdgSocket(*xdg);
  }
  throw ControlPathError(kMissingRuntimeMessage);
}

std::filesystem::path ResolveCli(const ControlPathQuery& query) {
  if (auto override_path = Env(kControlSocketEnv)) {
    return RequireAbsolute(*override_path);
  }
  auto system = query.system_socket.empty()
                    ? std::filesystem::path(kSystemControlSocket)
                    : query.system_socket;
  std::error_code ec;
  if (std::filesystem::exists(system, ec)) return system;
  if (auto xdg = Env(kXdgRuntimeDirEnv)) return XdgSocket(*xdg);
  throw ControlPathError(kMissingRuntimeMessage);
}

}  // namespace

std::filesystem::path ResolveControlSocketPath(ControlPathSide side,
                                               const ControlPathQuery& query) {
  if (side == ControlPathSide::kDaemon) return ResolveDaemon();
  return ResolveCli(query);
}

}  // namespace control
}  // namespace agent
}  // namespace volta
