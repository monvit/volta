#ifndef VOLTA_AGENT_CONTROL_CONTROL_SOCKET_H_
#define VOLTA_AGENT_CONTROL_CONTROL_SOCKET_H_

#include <sys/types.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "control/control_command.h"
#include "control/unique_fd.h"
#include "logger/logger.h"

namespace volta {
namespace agent {
namespace control {

inline constexpr std::size_t kMaxControlMessageSize = 64 * 1024;
inline constexpr int kControlPeerIdleTimeoutMs = 2000;
inline constexpr int kControlReplyTimeoutMs = 5000;

const char* DescribeSendError(int err);

struct ClientOptions {
  std::size_t max_message_size = kMaxControlMessageSize;
  int timeout_ms = kControlReplyTimeoutMs;
};

// Opens a fresh SOCK_SEQPACKET connection, sends message, and reads one reply.
// On failure returns std::nullopt and writes errno to err_out if non-null.
std::optional<ControlReply> SendRequest(const std::string& path,
                                        std::string_view message,
                                        int* err_out = nullptr,
                                        ClientOptions opts = ClientOptions());

class ControlSocket {
 public:
  struct Options {
    mode_t mode;
    std::size_t max_message_size;
    int peer_idle_timeout_ms;
    Options()
        : mode(0660),
          max_message_size(kMaxControlMessageSize),
          peer_idle_timeout_ms(kControlPeerIdleTimeoutMs) {}
  };

  using MessageHandler = std::function<ControlReply(std::string_view message)>;

  ControlSocket(std::string path, std::unique_ptr<logging::Logger> logger,
                Options opts = Options());
  ~ControlSocket();

  ControlSocket(const ControlSocket&) = delete;
  ControlSocket& operator=(const ControlSocket&) = delete;
  ControlSocket(ControlSocket&&) = delete;
  ControlSocket& operator=(ControlSocket&&) = delete;

  const std::string& path() const { return path_; }

  // Handler runs on the recv thread, off the collection loop.
  void Listen(MessageHandler handler);
  void StopListening();

 private:
  enum class PollResult { kReady, kTimeout, kStop };

  void BindServer();
  void UnlinkSocketFile();
  void RecvLoop();
  // Blocks until fd is readable, timeout_ms elapses, or StopListening() fires.
  PollResult WaitForFd(int fd, int timeout_ms);

  std::string path_;
  Options opts_;
  UniqueFd lock_fd_;
  UniqueFd fd_;
  UniqueFd wake_fd_;
  std::unique_ptr<logging::Logger> logger_;
  MessageHandler handler_;
  std::thread recv_thread_;
};

}  // namespace control
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CONTROL_CONTROL_SOCKET_H_
