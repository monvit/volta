#include "control/control_socket.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace volta {
namespace agent {
namespace control {
namespace {

class ScopedUmask {
 public:
  explicit ScopedUmask(mode_t mask) : old_(::umask(mask)) {}
  ~ScopedUmask() { ::umask(old_); }

  ScopedUmask(const ScopedUmask&) = delete;
  ScopedUmask& operator=(const ScopedUmask&) = delete;

 private:
  mode_t old_;
};

sockaddr_un MakeAddr(const std::string& path, socklen_t* len_out) {
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  if (path.size() >= sizeof(addr.sun_path)) {
    throw std::invalid_argument("control socket path too long");
  }
  std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
  *len_out =
      static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size() + 1);
  return addr;
}

}  // namespace

const char* DescribeSendError(int err) {
  switch (err) {
    case ENOENT:
      return "service not running (control socket missing)";
    case ECONNREFUSED:
      return "service not running (nothing listening on control socket)";
    case EACCES:
    case EPERM:
      return "permission denied (need root or membership of the socket group)";
    case EMSGSIZE:
      return "message too large for control socket";
    default:
      return std::strerror(err);
  }
}

ControlSocket::ControlSocket(std::string path, Role role, Options opts)
    : path_(std::move(path)), role_(role), opts_(opts) {
  if (path_.empty()) {
    throw std::invalid_argument("control socket path must not be empty");
  }
  if (opts_.max_message_size == 0) {
    throw std::invalid_argument("max_message_size must be > 0");
  }
  // Reject overlong paths before socket() so a throw does not leak an fd.
  socklen_t ignored = 0;
  MakeAddr(path_, &ignored);

  fd_ = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (fd_ < 0) {
    throw std::runtime_error(std::string("socket(AF_UNIX, SOCK_DGRAM): ") +
                             std::strerror(errno));
  }
  if (role_ == Role::kServer) BindServer();
}

ControlSocket::~ControlSocket() {
  StopListening();
  CloseFd();
  if (owns_path_) {
    // ENOENT is success: systemd may already have removed the runtime dir.
    if (::unlink(path_.c_str()) != 0 && errno != ENOENT) {
      std::cerr << "control socket unlink(" << path_
                << "): " << std::strerror(errno) << "\n";
    }
  }
}

void ControlSocket::CloseFd() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  connected_ = false;
}

void ControlSocket::BindServer() {
  // Stale inode from a crashed non-systemd run. Harmless if absent.
  if (::unlink(path_.c_str()) != 0 && errno != ENOENT) {
    int saved = errno;
    CloseFd();
    throw std::runtime_error(std::string("unlink(") + path_ +
                             "): " + std::strerror(saved));
  }

  socklen_t len = 0;
  sockaddr_un addr = MakeAddr(path_, &len);

  // Bind creates the inode as 0777 masked by umask. Tighten the umask for
  // the bind, then chmod to the exact mode.
  mode_t bind_umask = static_cast<mode_t>((~opts_.mode) & 0777);
  int bind_rc = -1;
  int bind_errno = 0;
  {
    ScopedUmask umask_guard(bind_umask);
    bind_rc = ::bind(fd_, reinterpret_cast<sockaddr*>(&addr), len);
    bind_errno = errno;
  }
  if (bind_rc != 0) {
    CloseFd();
    throw std::runtime_error(std::string("bind(") + path_ +
                             "): " + std::strerror(bind_errno));
  }
  owns_path_ = true;

  if (::chmod(path_.c_str(), opts_.mode) != 0) {
    int saved = errno;
    CloseFd();
    ::unlink(path_.c_str());
    owns_path_ = false;
    throw std::runtime_error(std::string("chmod(") + path_ +
                             "): " + std::strerror(saved));
  }

  int flags = ::fcntl(fd_, F_GETFL, 0);
  if (flags >= 0) ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
}

bool ControlSocket::EnsureClientConnected(int* err_out) {
  if (connected_) return true;
  socklen_t len = 0;
  sockaddr_un addr = MakeAddr(path_, &len);
  if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), len) != 0) {
    if (err_out) *err_out = errno;
    return false;
  }
  connected_ = true;
  return true;
}

bool ControlSocket::Send(std::string_view message, int* err_out) {
  if (role_ != Role::kClient) {
    throw std::logic_error("Send() called on a server control socket");
  }
  if (message.size() > opts_.max_message_size) {
    if (err_out) *err_out = EMSGSIZE;
    return false;
  }
  if (!EnsureClientConnected(err_out)) return false;

  ssize_t n = ::send(fd_, message.data(), message.size(), MSG_NOSIGNAL);
  if (n < 0 && (errno == ECONNREFUSED || errno == ENOTCONN || errno == EPIPE)) {
    connected_ = false;
    if (!EnsureClientConnected(err_out)) return false;
    n = ::send(fd_, message.data(), message.size(), MSG_NOSIGNAL);
  }
  if (n < 0) {
    if (err_out) *err_out = errno;
    return false;
  }
  if (static_cast<std::size_t>(n) != message.size()) {
    if (err_out) *err_out = EMSGSIZE;
    return false;
  }
  return true;
}

void ControlSocket::Listen(MessageHandler handler) {
  if (role_ != Role::kServer) {
    throw std::logic_error("Listen() called on a client control socket");
  }
  if (listening_.load()) return;
  handler_ = std::move(handler);
  listening_.store(true);
  recv_thread_ = std::thread([this] { RecvLoop(); });
}

void ControlSocket::StopListening() {
  listening_.store(false);
  if (recv_thread_.joinable()) recv_thread_.join();
}

namespace {

bool WaitForDatagram(int fd, const std::atomic<bool>& listening) {
  while (listening.load()) {
    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    int ready = ::poll(&pfd, 1, 200);
    if (ready < 0) {
      if (errno == EINTR) continue;
      if (listening.load()) {
        std::cerr << "control socket poll: " << std::strerror(errno) << "\n";
      }
      return false;
    }
    if (ready == 0) continue;
    return listening.load();
  }
  return false;
}

enum class DatagramRead { kRetry, kStop, kMessage };

DatagramRead ReadDatagram(int fd, std::vector<char>& buf, bool still_listening,
                          std::string* message) {
  ssize_t n = ::recv(fd, buf.data(), buf.size(), MSG_TRUNC);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return DatagramRead::kRetry;
    }
    if (still_listening) {
      std::cerr << "control socket recv: " << std::strerror(errno) << "\n";
    }
    return DatagramRead::kStop;
  }
  if (static_cast<std::size_t>(n) > buf.size()) {
    std::cerr << "control socket: dropped truncated datagram\n";
    return DatagramRead::kRetry;
  }
  message->assign(buf.data(), static_cast<std::size_t>(n));
  return DatagramRead::kMessage;
}

}  // namespace

void ControlSocket::RecvLoop() {
  std::vector<char> buf(opts_.max_message_size);
  while (WaitForDatagram(fd_, listening_)) {
    std::string message;
    switch (ReadDatagram(fd_, buf, listening_.load(), &message)) {
      case DatagramRead::kRetry:
        continue;
      case DatagramRead::kStop:
        return;
      case DatagramRead::kMessage:
        break;
    }
    if (!handler_) continue;
    try {
      handler_(message);
    } catch (const std::exception& ex) {
      std::cerr << "control socket handler: " << ex.what() << "\n";
    }
  }
}

}  // namespace control
}  // namespace agent
}  // namespace volta
