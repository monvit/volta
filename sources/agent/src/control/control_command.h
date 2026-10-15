#ifndef VOLTA_AGENT_CONTROL_CONTROL_COMMAND_H_
#define VOLTA_AGENT_CONTROL_CONTROL_COMMAND_H_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace volta {
namespace agent {
namespace control {

struct DumpStart {
  std::optional<std::filesystem::path> path;
};

struct DumpEnd {};

using ControlCommand = std::variant<DumpStart, DumpEnd>;

std::string Encode(const ControlCommand& command);
std::optional<ControlCommand> Decode(std::string_view payload);

}  // namespace control
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CONTROL_CONTROL_COMMAND_H_
