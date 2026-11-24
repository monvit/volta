#ifndef VOLTA_AGENT_CONTROL_CONTROL_SOCKET_H_
#define VOLTA_AGENT_CONTROL_CONTROL_SOCKET_H_

#include <sys/types.h>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "control/unique_fd.h"
#include "logger/logger.h"

namespace volta {
namespace agent {
namespace control {

const char* DescribeSendError(int err);

class ControlSocket {
 public:
  enum class Role { kServer, kClient };

  struct Options {
    mode_t mode;
    std::size_t max_message_size;
    Options() : mode(0660), max_message_size(64 * 1024) {}
  };

  using MessageHandler = std::function<void(std::string_view message)>;

  ControlSocket(std::string path, Role role,
                std::unique_ptr<logging::Logger> logger,
                Options opts = Options());
  ~ControlSocket();

  ControlSocket(const ControlSocket&) = delete;
  ControlSocket& operator=(const ControlSocket&) = delete;
  ControlSocket(ControlSocket&&) = delete;
  ControlSocket& operator=(ControlSocket&&) = delete;

  const std::string& path() const { return path_; }
  Role role() const { return role_; }

  // Oversized payloads set EMSGSIZE and are not sent.
  bool Send(std::string_view message, int* err_out = nullptr);

  // Handler runs on the recv thread, off the collection loop.
  void Listen(MessageHandler handler);
  void StopListening();

 private:
  void BindServer();
  bool EnsureClientConnected(int* err_out);
  void RecvLoop();
  enum class DatagramRead { kRetry, kStop, kMessage };
  // Blocks until fd_ is readable or StopListening() signals wake_fd_.
  bool WaitForDatagram();
  DatagramRead ReadDatagram(std::vector<char>& buf, std::string* message);

  std::string path_;
  Role role_;
  Options opts_;
  // Closed before path_ is unlinked. lock_fd_ stays open across that unlink
  // and is released only when members are destroyed.
  UniqueFd lock_fd_;
  UniqueFd fd_;
  UniqueFd wake_fd_;
  bool connected_ = false;
  bool owns_path_ = false;
  std::unique_ptr<logging::Logger> logger_;
  MessageHandler handler_;
  std::thread recv_thread_;
};

}  // namespace control
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CONTROL_CONTROL_SOCKET_H_
