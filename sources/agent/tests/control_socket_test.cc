#include "control/control_socket.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "control/control_command.h"
#include "control/control_socket_path.h"
#include "gtest/gtest.h"

namespace volta {
namespace agent {
namespace control {
namespace {

class ControlSocketTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Save(kControlSocketEnv);
    Save(kRuntimeDirectoryEnv);
    Save(kXdgRuntimeDirEnv);
    unsetenv(kControlSocketEnv);
    unsetenv(kRuntimeDirectoryEnv);
    unsetenv(kXdgRuntimeDirEnv);
    dir_ = std::filesystem::temp_directory_path() /
           ("volta-ctl-" + std::to_string(::getpid()) + "-" +
            std::to_string(++counter_));
    std::filesystem::create_directories(dir_);
    query_.system_socket = dir_ / "absent-system.sock";
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
    Restore();
  }

  void Save(const char* key) {
    if (const char* value = std::getenv(key)) {
      saved_.emplace_back(key, std::string(value));
    } else {
      saved_.emplace_back(key, std::nullopt);
    }
  }

  void Restore() {
    for (const auto& [key, value] : saved_) {
      if (value) {
        setenv(key.c_str(), value->c_str(), 1);
      } else {
        unsetenv(key.c_str());
      }
    }
  }

  std::filesystem::path dir_;
  ControlPathQuery query_;

 private:
  inline static int counter_ = 0;
  std::vector<std::pair<std::string, std::optional<std::string>>> saved_;
};

TEST_F(ControlSocketTest, RoundTrip) {
  auto path = (dir_ / "control.sock").string();
  ControlSocket server(path, ControlSocket::Role::kServer);

  std::mutex mu;
  std::condition_variable cv;
  std::optional<ControlCommand> got;
  server.Listen([&](std::string_view message) {
    std::lock_guard lock(mu);
    got = Decode(message);
    cv.notify_one();
  });

  ControlSocket client(path, ControlSocket::Role::kClient);
  int err = 0;
  ASSERT_TRUE(client.Send(Encode(DumpEnd{}), &err)) << DescribeSendError(err);

  std::unique_lock lock(mu);
  ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(2),
                          [&] { return got.has_value(); }));
  EXPECT_TRUE(std::holds_alternative<DumpEnd>(*got));
}

TEST_F(ControlSocketTest, SocketModeIs0660AndUnlinks) {
  auto path = dir_ / "control.sock";
  {
    ControlSocket server(path.string(), ControlSocket::Role::kServer);
    struct stat st{};
    ASSERT_EQ(::stat(path.c_str(), &st), 0);
    EXPECT_EQ(st.st_mode & 0777, 0660);
  }
  EXPECT_FALSE(std::filesystem::exists(path));
  ControlSocket again(path.string(), ControlSocket::Role::kServer);
  EXPECT_TRUE(std::filesystem::is_socket(path));
}

TEST_F(ControlSocketTest, MissingSocketSaysServiceDown) {
  ControlSocket client((dir_ / "missing.sock").string(),
                       ControlSocket::Role::kClient);
  int err = 0;
  EXPECT_FALSE(client.Send("dump_end", &err));
  EXPECT_EQ(err, ENOENT);
  EXPECT_NE(std::string(DescribeSendError(err)).find("service not running"),
            std::string::npos);
  EXPECT_NE(std::string(DescribeSendError(EACCES)).find("permission denied"),
            std::string::npos);
}

TEST_F(ControlSocketTest, DropsTruncatedDatagram) {
  auto path = (dir_ / "control.sock").string();
  ControlSocket::Options opts;
  opts.max_message_size = 8;
  ControlSocket server(path, ControlSocket::Role::kServer, opts);

  std::mutex mu;
  std::condition_variable cv;
  std::string got;
  server.Listen([&](std::string_view message) {
    std::lock_guard lock(mu);
    got.assign(message);
    cv.notify_one();
  });

  int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  ASSERT_GE(fd, 0);
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
  socklen_t len =
      static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size() + 1);
  ASSERT_EQ(::connect(fd, reinterpret_cast<sockaddr*>(&addr), len), 0);
  std::string big(64, 'x');
  ASSERT_EQ(::send(fd, big.data(), big.size(), MSG_NOSIGNAL),
            static_cast<ssize_t>(big.size()));
  ASSERT_EQ(::send(fd, "ping", 4, MSG_NOSIGNAL), 4);
  ::close(fd);

  std::unique_lock lock(mu);
  ASSERT_TRUE(
      cv.wait_for(lock, std::chrono::seconds(2), [&] { return !got.empty(); }));
  EXPECT_EQ(got, "ping");
}

TEST_F(ControlSocketTest, DaemonPath) {
  auto runtime = dir_ / "runtime";
  std::filesystem::create_directory(runtime);
  setenv(kRuntimeDirectoryEnv, runtime.c_str(), 1);
  EXPECT_EQ(ResolveControlSocketPath(ControlPathSide::kDaemon),
            runtime / "control.sock");

  auto missing = dir_ / "missing-runtime";
  setenv(kRuntimeDirectoryEnv, missing.c_str(), 1);
  EXPECT_THROW(ResolveControlSocketPath(ControlPathSide::kDaemon),
               ControlPathError);
  EXPECT_FALSE(std::filesystem::exists(missing));

  unsetenv(kRuntimeDirectoryEnv);
  auto xdg = dir_ / "xdg";
  std::filesystem::create_directory(xdg);
  setenv(kXdgRuntimeDirEnv, xdg.c_str(), 1);
  EXPECT_EQ(ResolveControlSocketPath(ControlPathSide::kDaemon),
            xdg / "volta" / "control.sock");
  struct stat st{};
  ASSERT_EQ(::stat((xdg / "volta").c_str(), &st), 0);
  EXPECT_EQ(st.st_mode & 0777, 0700);

  auto socket = dir_ / "override" / "control.sock";
  setenv(kControlSocketEnv, socket.c_str(), 1);
  setenv(kRuntimeDirectoryEnv, runtime.c_str(), 1);
  EXPECT_EQ(ResolveControlSocketPath(ControlPathSide::kDaemon), socket);

  setenv(kControlSocketEnv, "relative.sock", 1);
  EXPECT_THROW(ResolveControlSocketPath(ControlPathSide::kDaemon),
               ControlPathError);

  unsetenv(kControlSocketEnv);
  unsetenv(kRuntimeDirectoryEnv);
  unsetenv(kXdgRuntimeDirEnv);
  try {
    ResolveControlSocketPath(ControlPathSide::kDaemon);
    FAIL() << "expected ControlPathError";
  } catch (const ControlPathError& err) {
    std::string message = err.what();
    EXPECT_NE(message.find("$VOLTA_CONTROL_SOCKET"), std::string::npos);
    EXPECT_NE(message.find("$RUNTIME_DIRECTORY"), std::string::npos);
    EXPECT_NE(message.find("$XDG_RUNTIME_DIR"), std::string::npos);
  }
}

TEST_F(ControlSocketTest, CliPath) {
  auto system = dir_ / "system.sock";
  ControlSocket server(system.string(), ControlSocket::Role::kServer);
  auto xdg = dir_ / "xdg";
  std::filesystem::create_directory(xdg);
  setenv(kXdgRuntimeDirEnv, xdg.c_str(), 1);
  query_.system_socket = system;
  EXPECT_EQ(ResolveControlSocketPath(ControlPathSide::kCli, query_), system);
  EXPECT_FALSE(std::filesystem::exists(xdg / "volta"));

  query_.system_socket = dir_ / "absent-system.sock";
  EXPECT_EQ(ResolveControlSocketPath(ControlPathSide::kCli, query_),
            xdg / "volta" / "control.sock");
  EXPECT_FALSE(std::filesystem::exists(xdg / "volta"));

  unsetenv(kXdgRuntimeDirEnv);
  auto runtime = dir_ / "runtime";
  std::filesystem::create_directory(runtime);
  setenv(kRuntimeDirectoryEnv, runtime.c_str(), 1);
  EXPECT_THROW(ResolveControlSocketPath(ControlPathSide::kCli, query_),
               ControlPathError);
}

}  // namespace
}  // namespace control
}  // namespace agent
}  // namespace volta
