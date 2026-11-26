#ifndef VOLTA_AGENT_CONTROL_UNIQUE_FD_H_
#define VOLTA_AGENT_CONTROL_UNIQUE_FD_H_

#include <unistd.h>

namespace volta {
namespace agent {
namespace control {

// Move-only RAII owner for one POSIX file descriptor.
class UniqueFd {
 public:
  UniqueFd() = default;
  explicit UniqueFd(int fd) noexcept : fd_(fd) {}
  ~UniqueFd() { reset(); }

  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;

  UniqueFd(UniqueFd&& other) noexcept : fd_(other.release()) {}
  UniqueFd& operator=(UniqueFd&& other) noexcept {
    if (this != &other) reset(other.release());
    return *this;
  }

  int get() const noexcept { return fd_; }
  explicit operator bool() const noexcept { return fd_ >= 0; }

  int release() noexcept {
    int fd = fd_;
    fd_ = -1;
    return fd;
  }

  void reset(int fd = -1) noexcept {
    if (fd_ >= 0) {
      // close(2) releases the descriptor on Linux even when it returns EINTR.
      // Retrying could close a descriptor the kernel has already reused.
      (void)::close(fd_);
    }
    fd_ = fd;
  }

 private:
  int fd_ = -1;
};

}  // namespace control
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CONTROL_UNIQUE_FD_H_
