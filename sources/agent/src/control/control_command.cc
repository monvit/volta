#include "control/control_command.h"

namespace volta {
namespace agent {
namespace control {

std::string Encode(const ControlCommand& command) {
  if (const auto* start = std::get_if<DumpStart>(&command)) {
    if (!start->path || start->path->empty()) return "dump_start";
    return "dump_start;" + start->path->string();
  }
  return "dump_end";
}

std::optional<ControlCommand> Decode(std::string_view payload) {
  if (payload == "dump_end") return DumpEnd{};
  constexpr std::string_view kPrefix = "dump_start;";
  if (payload == "dump_start" || payload == kPrefix) return DumpStart{};
  if (!payload.starts_with(kPrefix)) return std::nullopt;
  return DumpStart{
      std::filesystem::path(std::string(payload.substr(kPrefix.size())))};
}

std::string Encode(const ControlReply& reply) {
  return std::string(reply.ok ? "ok\n" : "err\n") + reply.message;
}

std::optional<ControlReply> DecodeReply(std::string_view payload) {
  const auto split = payload.find('\n');
  if (split == std::string_view::npos) return std::nullopt;

  const auto status = payload.substr(0, split);
  const auto message = payload.substr(split + 1);
  if (status == "ok") return ControlReply{true, std::string(message)};
  if (status == "err") return ControlReply{false, std::string(message)};
  return std::nullopt;
}

}  // namespace control
}  // namespace agent
}  // namespace volta
