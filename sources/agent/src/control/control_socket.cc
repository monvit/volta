#include "control/control_socket.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>
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

ControlSocket::ControlSocket(std::string path, Role role,
                             std::unique_ptr<logging::Logger> logger,
                             Options opts)
    : path_(std::move(path)),
      role_(role),
      opts_(opts),
      logger_(std::move(logger)) {
  if (path_.empty()) {
    throw std::invalid_argument("control socket path must not be empty");
  }
  if (opts_.max_message_size == 0) {
    throw std::invalid_argument("max_message_size must be > 0");
  }
  // Reject overlong paths before creating the socket.
  socklen_t ignored = 0;
  MakeAddr(path_, &ignored);

  fd_ = UniqueFd(::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0));
  if (!fd_) {
    throw std::runtime_error(std::string("socket(AF_UNIX, SOCK_DGRAM): ") +
                             std::strerror(errno));
  }
  if (role_ == Role::kServer) BindServer();
}

ControlSocket::~ControlSocket() {
  // Join the receive thread before closing the socket. Unlink while the
  // lock fd is still held; lock_fd_ closes later, during member destruction.
  StopListening();
  fd_.reset();
  if (owns_path_) {
    // ENOENT is success: systemd may already have removed the runtime dir.
    if (::unlink(path_.c_str()) != 0 && errno != ENOENT) {
      logger_->Log(logging::LogLevel::ERROR_LEVEL,
                   "control socket unlink({}): {}", path_,
                   std::strerror(errno));
    }
    owns_path_ = false;
  }
}

void ControlSocket::BindServer() {
  const std::string lock_path = path_ + ".lock";
  lock_fd_ =
      UniqueFd(::open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0660));
  if (!lock_fd_) {
    int saved = errno;
    throw std::runtime_error(std::string("open(") + lock_path +
                             "): " + std::strerror(saved));
  }
  if (::flock(lock_fd_.get(), LOCK_EX | LOCK_NB) != 0) {
    int saved = errno;
    if (saved == EWOULDBLOCK || saved == EAGAIN) {
      throw std::runtime_error("control socket already running");
    }
    throw std::runtime_error(std::string("flock(") + lock_path +
                             "): " + std::strerror(saved));
  }

  // Stale inode from a crashed non-systemd run. Harmless if absent.
  if (::unlink(path_.c_str()) != 0 && errno != ENOENT) {
    int saved = errno;
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
    bind_rc = ::bind(fd_.get(), reinterpret_cast<sockaddr*>(&addr), len);
    bind_errno = errno;
  }
  if (bind_rc != 0) {
    throw std::runtime_error(std::string("bind(") + path_ +
                             "): " + std::strerror(bind_errno));
  }
  owns_path_ = true;

  if (::chmod(path_.c_str(), opts_.mode) != 0) {
    int saved = errno;
    // Keep the single-instance lock held while removing the partial socket
    // so another daemon cannot bind this path in between. lock_fd_ stays
    // open until member destruction after this throw.
    fd_.reset();
    if (::unlink(path_.c_str()) != 0 && errno != ENOENT) {
      logger_->Log(logging::LogLevel::ERROR_LEVEL,
                   "control socket unlink({}): {}", path_,
                   std::strerror(errno));
    }
    owns_path_ = false;
    throw std::runtime_error(std::string("chmod(") + path_ +
                             "): " + std::strerror(saved));
  }

  int flags = ::fcntl(fd_.get(), F_GETFL, 0);
  if (flags >= 0) ::fcntl(fd_.get(), F_SETFL, flags | O_NONBLOCK);
}

bool ControlSocket::EnsureClientConnected(int* err_out) {
  if (connected_) return true;
  socklen_t len = 0;
  sockaddr_un addr = MakeAddr(path_, &len);
  if (::connect(fd_.get(), reinterpret_cast<sockaddr*>(&addr), len) != 0) {
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

  ssize_t n = ::send(fd_.get(), message.data(), message.size(), MSG_NOSIGNAL);
  if (n < 0 && (errno == ECONNREFUSED || errno == ENOTCONN || errno == EPIPE)) {
    connected_ = false;
    if (!EnsureClientConnected(err_out)) return false;
    n = ::send(fd_.get(), message.data(), message.size(), MSG_NOSIGNAL);
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
  if (recv_thread_.joinable()) return;
  handler_ = std::move(handler);
  wake_fd_ = UniqueFd(::eventfd(0, EFD_CLOEXEC));
  if (!wake_fd_) {
    int saved = errno;
    throw std::runtime_error(std::string("eventfd: ") + std::strerror(saved));
  }
  recv_thread_ = std::thread([this] { RecvLoop(); });
}

void ControlSocket::StopListening() {
  if (!recv_thread_.joinable()) return;
  ::eventfd_write(wake_fd_.get(), 1);
  recv_thread_.join();
  wake_fd_.reset();
}

bool ControlSocket::WaitForDatagram() {
  pollfd pfds[2]{};
  pfds[0].fd = fd_.get();
  pfds[0].events = POLLIN;
  pfds[1].fd = wake_fd_.get();
  pfds[1].events = POLLIN;

  while (true) {
    int ready = ::poll(pfds, 2, -1);
    if (ready < 0) {
      if (errno == EINTR) continue;
      logger_->Log(logging::LogLevel::ERROR_LEVEL, "control socket poll: {}",
                   std::strerror(errno));
      return false;
    }
    // Shutdown wins over a datagram that arrived in the same wake.
    if (pfds[1].revents != 0) return false;
    if (pfds[0].revents != 0) return true;
  }
}

ControlSocket::DatagramRead ControlSocket::ReadDatagram(std::vector<char>& buf,
                                                        std::string* message) {
  ssize_t n = ::recv(fd_.get(), buf.data(), buf.size(), MSG_TRUNC);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      return DatagramRead::kRetry;
    }
    logger_->Log(logging::LogLevel::ERROR_LEVEL, "control socket recv: {}",
                 std::strerror(errno));
    return DatagramRead::kStop;
  }
  if (static_cast<std::size_t>(n) > buf.size()) {
    logger_->Log(logging::LogLevel::ERROR_LEVEL,
                 "control socket: dropped truncated datagram");
    return DatagramRead::kRetry;
  }
  message->assign(buf.data(), static_cast<std::size_t>(n));
  return DatagramRead::kMessage;
}

void ControlSocket::RecvLoop() {
  std::vector<char> buf(opts_.max_message_size);
  while (WaitForDatagram()) {
    std::string message;
    switch (ReadDatagram(buf, &message)) {
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
      logger_->Log(logging::LogLevel::ERROR_LEVEL, "control socket handler: {}",
                   ex.what());
    }
  }
}

}  // namespace control
}  // namespace agent
}  // namespace volta
