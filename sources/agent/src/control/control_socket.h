#ifndef VOLTA_AGENT_CONTROL_CONTROL_SOCKET_H_
#define VOLTA_AGENT_CONTROL_CONTROL_SOCKET_H_

#include <sys/types.h>

#include <atomic>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <thread>

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

  ControlSocket(std::string path, Role role, Options opts = Options());
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
  void CloseFd();
  void BindServer();
  bool EnsureClientConnected(int* err_out);
  void RecvLoop();

  std::string path_;
  Role role_;
  Options opts_;
  int fd_ = -1;
  bool connected_ = false;
  bool owns_path_ = false;
  MessageHandler handler_;
  std::atomic<bool> listening_{false};
  std::thread recv_thread_;
};

}  // namespace control
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CONTROL_CONTROL_SOCKET_H_
