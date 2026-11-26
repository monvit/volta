#include "control/control_socket.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace volta {
namespace agent {
namespace control {
namespace {

constexpr int kListenBacklog = 16;
constexpr int kAcceptBackoffMs = 100;

template <typename Call>
auto RetryOnEintr(Call call) {
  decltype(call()) rc;
  do {
    rc = call();
  } while (rc < 0 && errno == EINTR);
  return rc;
}

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

int MapTimeoutErrno(int err) {
  if (err == EAGAIN || err == EWOULDBLOCK) return ETIMEDOUT;
  return err;
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
    case ETIMEDOUT:
      return "timed out waiting for the daemon";
    case ECONNRESET:
      return "daemon closed the connection without replying";
    case EBADMSG:
      return "invalid reply from daemon";
    default:
      return std::strerror(err);
  }
}

std::optional<ControlReply> SendRequest(const std::string& path,
                                        std::string_view message, int* err_out,
                                        ClientOptions opts) {
  auto fail = [&](int err) -> std::optional<ControlReply> {
    if (err_out) *err_out = err;
    return std::nullopt;
  };

  if (path.empty() || message.empty() || opts.max_message_size == 0 ||
      opts.timeout_ms <= 0) {
    return fail(EINVAL);
  }
  if (path.size() >= sizeof(sockaddr_un::sun_path)) {
    return fail(ENAMETOOLONG);
  }
  if (message.size() > opts.max_message_size) {
    return fail(EMSGSIZE);
  }

  UniqueFd fd(::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0));
  if (!fd) return fail(errno);

  // SO_SNDTIMEO / SO_RCVTIMEO apply per syscall step rather than as a single
  // total wall-clock budget.
  timeval tv{};
  tv.tv_sec = opts.timeout_ms / 1000;
  tv.tv_usec = (opts.timeout_ms % 1000) * 1000;
  if (::setsockopt(fd.get(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0 ||
      ::setsockopt(fd.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
    return fail(errno);
  }

  socklen_t len = 0;
  sockaddr_un addr = MakeAddr(path, &len);
  if (RetryOnEintr([&] {
        return ::connect(fd.get(), reinterpret_cast<sockaddr*>(&addr), len);
      }) != 0) {
    return fail(MapTimeoutErrno(errno));
  }

  if (RetryOnEintr([&] {
        return ::send(fd.get(), message.data(), message.size(), MSG_NOSIGNAL);
      }) < 0) {
    return fail(MapTimeoutErrno(errno));
  }

  std::vector<char> buf(opts.max_message_size);
  const ssize_t n = RetryOnEintr(
      [&] { return ::recv(fd.get(), buf.data(), buf.size(), MSG_TRUNC); });
  if (n < 0) return fail(MapTimeoutErrno(errno));
  if (n == 0) return fail(ECONNRESET);
  if (static_cast<std::size_t>(n) > buf.size()) return fail(EMSGSIZE);

  auto reply =
      DecodeReply(std::string_view(buf.data(), static_cast<std::size_t>(n)));
  if (!reply) return fail(EBADMSG);
  return reply;
}

ControlSocket::ControlSocket(std::string path,
                             std::unique_ptr<logging::Logger> logger,
                             Options opts)
    : path_(std::move(path)), opts_(opts), logger_(std::move(logger)) {
  if (path_.empty()) {
    throw std::invalid_argument("control socket path must not be empty");
  }
  if (opts_.max_message_size == 0) {
    throw std::invalid_argument("max_message_size must be > 0");
  }
  if (opts_.peer_idle_timeout_ms <= 0) {
    throw std::invalid_argument("peer_idle_timeout_ms must be > 0");
  }

  fd_ = UniqueFd(
      ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0));
  if (!fd_) {
    throw std::runtime_error(std::string("socket(AF_UNIX, SOCK_SEQPACKET): ") +
                             std::strerror(errno));
  }
  BindServer();
}

ControlSocket::~ControlSocket() {
  StopListening();
  UnlinkSocketFile();
}

// lock_fd_ is a member, so it is closed only after this runs (also on the
// constructor-throw path), keeping the single-instance lock held across unlink.
void ControlSocket::UnlinkSocketFile() {
  // ENOENT is success: systemd may already have removed the runtime dir.
  if (::unlink(path_.c_str()) != 0 && errno != ENOENT) {
    logger_->Log(logging::LogLevel::ERROR_LEVEL,
                 "control socket unlink({}): {}", path_, std::strerror(errno));
  }
}

void ControlSocket::BindServer() {
  socklen_t len = 0;
  sockaddr_un addr = MakeAddr(path_, &len);

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

  if (::chmod(path_.c_str(), opts_.mode) != 0) {
    int saved = errno;
    UnlinkSocketFile();
    throw std::runtime_error(std::string("chmod(") + path_ +
                             "): " + std::strerror(saved));
  }

  if (::listen(fd_.get(), kListenBacklog) != 0) {
    int saved = errno;
    UnlinkSocketFile();
    throw std::runtime_error(std::string("listen(") + path_ +
                             "): " + std::strerror(saved));
  }
}

void ControlSocket::Listen(MessageHandler handler) {
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

ControlSocket::PollResult ControlSocket::WaitForFd(int fd, int timeout_ms) {
  pollfd pfds[2]{};
  pfds[0].fd = fd;
  pfds[0].events = POLLIN;
  pfds[1].fd = wake_fd_.get();
  pfds[1].events = POLLIN;

  const auto deadline = timeout_ms > 0
                            ? std::chrono::steady_clock::now() +
                                  std::chrono::milliseconds(timeout_ms)
                            : std::chrono::steady_clock::time_point{};

  while (true) {
    int remaining_ms = timeout_ms;
    if (timeout_ms > 0) {
      const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                            deadline - std::chrono::steady_clock::now())
                            .count();
      if (left <= 0) return PollResult::kTimeout;
      remaining_ms = static_cast<int>(left);
    }

    int ready = ::poll(pfds, 2, remaining_ms);
    if (ready < 0) {
      if (errno == EINTR) continue;
      logger_->Log(logging::LogLevel::ERROR_LEVEL, "control socket poll: {}",
                   std::strerror(errno));
      return PollResult::kStop;
    }
    if (ready == 0) return PollResult::kTimeout;
    // Shutdown wins over a peer event that arrived in the same wake.
    if (pfds[1].revents != 0) return PollResult::kStop;
    if (pfds[0].revents != 0) return PollResult::kReady;
  }
}

void ControlSocket::RecvLoop() {
  std::vector<char> buf(opts_.max_message_size);
  while (WaitForFd(fd_.get(), -1) == PollResult::kReady) {
    UniqueFd conn(::accept4(fd_.get(), nullptr, nullptr, SOCK_CLOEXEC));
    if (!conn) {
      const int err = errno;
      if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK ||
          err == ECONNABORTED) {
        continue;
      }
      logger_->Log(logging::LogLevel::ERROR_LEVEL, "control socket accept4: {}",
                   std::strerror(err));
      pollfd wake_pfd{wake_fd_.get(), POLLIN, 0};
      if (::poll(&wake_pfd, 1, kAcceptBackoffMs) > 0 && wake_pfd.revents != 0) {
        return;
      }
      continue;
    }

    PollResult wait = WaitForFd(conn.get(), opts_.peer_idle_timeout_ms);
    if (wait == PollResult::kStop) return;
    if (wait == PollResult::kTimeout) continue;

    const ssize_t n = RetryOnEintr(
        [&] { return ::recv(conn.get(), buf.data(), buf.size(), MSG_TRUNC); });
    if (n <= 0) {
      if (n < 0) {
        logger_->Log(logging::LogLevel::ERROR_LEVEL, "control socket recv: {}",
                     std::strerror(errno));
      }
      continue;
    }

    ControlReply reply{false, "Command too large"};
    if (static_cast<std::size_t>(n) > buf.size()) {
      logger_->Log(logging::LogLevel::WARNING_LEVEL,
                   "control socket: rejected truncated command");
    } else {
      try {
        reply =
            handler_(std::string_view(buf.data(), static_cast<std::size_t>(n)));
      } catch (const std::exception& ex) {
        logger_->Log(logging::LogLevel::ERROR_LEVEL,
                     "control socket handler: {}", ex.what());
        reply = {false, "Internal error"};
      } catch (...) {
        logger_->Log(logging::LogLevel::ERROR_LEVEL,
                     "control socket handler: unknown exception");
        reply = {false, "Internal error"};
      }
    }

    const std::string encoded = Encode(reply);
    if (RetryOnEintr([&] {
          return ::send(conn.get(), encoded.data(), encoded.size(),
                        MSG_NOSIGNAL);
        }) < 0) {
      logger_->Log(logging::LogLevel::WARNING_LEVEL, "control socket send: {}",
                   std::strerror(errno));
    }
  }
}

}  // namespace control
}  // namespace agent
}  // namespace volta
