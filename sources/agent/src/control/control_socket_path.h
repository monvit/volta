#ifndef VOLTA_AGENT_CONTROL_CONTROL_SOCKET_PATH_H_
#define VOLTA_AGENT_CONTROL_CONTROL_SOCKET_PATH_H_

#include <filesystem>
#include <stdexcept>
#include <string>

namespace volta {
namespace agent {
namespace control {

inline constexpr char kControlSocketEnv[] = "VOLTA_CONTROL_SOCKET";
inline constexpr char kRuntimeDirectoryEnv[] = "RUNTIME_DIRECTORY";
inline constexpr char kXdgRuntimeDirEnv[] = "XDG_RUNTIME_DIR";
inline constexpr char kSystemControlSocket[] = "/run/volta/control.sock";

struct ControlPathError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

enum class ControlPathSide { kDaemon, kCli };

// The CLI uses system_socket when its directory exists; the daemon ignores it.
std::filesystem::path ResolveControlSocketPath(
    ControlPathSide side,
    const std::filesystem::path& system_socket = kSystemControlSocket);

}  // namespace control
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CONTROL_CONTROL_SOCKET_PATH_H_
