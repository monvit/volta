#include "control/control_socket.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "control/control_command.h"
#include "control/control_socket_path.h"
#include "gtest/gtest.h"
#include "logger/stderr_logger.h"

namespace volta {
namespace agent {
namespace control {
namespace {

sockaddr_un MakeUnixAddr(const std::string& path, socklen_t* len_out) {
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
  *len_out =
      static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size() + 1);
  return addr;
}

UniqueFd ConnectSeqpacket(const std::string& path) {
  UniqueFd fd(::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0));
  if (!fd) return {};
  socklen_t len = 0;
  sockaddr_un addr = MakeUnixAddr(path, &len);
  if (::connect(fd.get(), reinterpret_cast<sockaddr*>(&addr), len) != 0) {
    return {};
  }
  return fd;
}

class ControlSocketTest : public ::testing::Test {
 protected:
  std::unique_ptr<logging::Logger> MakeLogger() const {
    logging::StderrLogger logger;
    return logger.WithName("ControlSocketTest");
  }

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

 private:
  inline static int counter_ = 0;
  std::vector<std::pair<std::string, std::optional<std::string>>> saved_;
};

TEST_F(ControlSocketTest, RoundTrip) {
  auto path = (dir_ / "control.sock").string();
  ControlSocket server(path, MakeLogger());

  server.Listen([](std::string_view message) -> ControlReply {
    auto cmd = Decode(message);
    if (!cmd) return {false, "Unrecognized command"};
    if (std::holds_alternative<DumpEnd>(*cmd)) {
      return {true, "No dump was running"};
    }
    return {false, "Dump already running: /tmp/a.csv"};
  });

  int err = 0;
  auto ok_reply = SendRequest(path, Encode(DumpEnd{}), &err);
  ASSERT_TRUE(ok_reply.has_value()) << DescribeSendError(err);
  EXPECT_TRUE(ok_reply->ok);
  EXPECT_EQ(ok_reply->message, "No dump was running");

  auto err_reply = SendRequest(path, Encode(DumpStart{}), &err);
  ASSERT_TRUE(err_reply.has_value()) << DescribeSendError(err);
  EXPECT_FALSE(err_reply->ok);
  EXPECT_EQ(err_reply->message, "Dump already running: /tmp/a.csv");
}

TEST_F(ControlSocketTest, SocketModeIs0660AndUnlinks) {
  auto path = dir_ / "control.sock";
  {
    ControlSocket server(path.string(), MakeLogger());
    struct stat st{};
    ASSERT_EQ(::stat(path.c_str(), &st), 0);
    EXPECT_EQ(st.st_mode & 0777, 0660);
  }
  EXPECT_FALSE(std::filesystem::exists(path));
  EXPECT_TRUE(
      std::filesystem::exists(std::filesystem::path(path.string() + ".lock")));
  ControlSocket again(path.string(), MakeLogger());
  EXPECT_TRUE(std::filesystem::is_socket(path));
}

TEST_F(ControlSocketTest, SecondServerOnSamePathThrowsAlreadyRunning) {
  auto path = (dir_ / "control.sock").string();
  ControlSocket server(path, MakeLogger());

  server.Listen([](std::string_view) -> ControlReply { return {true, "ok"}; });

  try {
    ControlSocket overlapping(path, MakeLogger());
    FAIL() << "expected second server to throw";
  } catch (const std::runtime_error& err) {
    EXPECT_NE(std::string(err.what()).find("already running"),
              std::string::npos);
  }

  int err = 0;
  auto reply = SendRequest(path, Encode(DumpEnd{}), &err);
  ASSERT_TRUE(reply.has_value()) << DescribeSendError(err);
  EXPECT_TRUE(reply->ok);
  EXPECT_EQ(reply->message, "ok");
}

TEST_F(ControlSocketTest, MissingSocketSaysServiceDown) {
  int err = 0;
  EXPECT_FALSE(SendRequest((dir_ / "missing.sock").string(), "dump_end", &err));
  EXPECT_EQ(err, ENOENT);
  EXPECT_NE(std::string(DescribeSendError(err)).find("service not running"),
            std::string::npos);
  EXPECT_NE(std::string(DescribeSendError(EACCES)).find("permission denied"),
            std::string::npos);
  EXPECT_NE(std::string(DescribeSendError(ETIMEDOUT)).find("timed out"),
            std::string::npos);
  EXPECT_NE(
      std::string(DescribeSendError(ECONNRESET)).find("closed the connection"),
      std::string::npos);
  EXPECT_NE(std::string(DescribeSendError(EBADMSG)).find("invalid reply"),
            std::string::npos);
}

TEST_F(ControlSocketTest, ClientRejectsEmptyAndOversizedPayloads) {
  int err = 0;
  EXPECT_FALSE(SendRequest((dir_ / "missing.sock").string(), "", &err));
  EXPECT_EQ(err, EINVAL);

  ClientOptions opts;
  opts.max_message_size = 8;
  err = 0;
  EXPECT_FALSE(SendRequest((dir_ / "missing.sock").string(),
                           std::string(16, 'x'), &err, opts));
  EXPECT_EQ(err, EMSGSIZE);
}

TEST_F(ControlSocketTest, ServerRepliesCommandTooLargeForTruncatedCommand) {
  auto path = (dir_ / "control.sock").string();
  ControlSocket::Options opts;
  opts.max_message_size = 8;
  ControlSocket server(path, MakeLogger(), opts);

  server.Listen([](std::string_view message) -> ControlReply {
    return {true, std::string(message)};
  });

  int err = 0;
  auto big_reply = SendRequest(path, std::string(64, 'x'), &err);
  ASSERT_TRUE(big_reply.has_value()) << DescribeSendError(err);
  EXPECT_FALSE(big_reply->ok);
  EXPECT_EQ(big_reply->message, "Command too large");

  auto ok_reply = SendRequest(path, "ping", &err);
  ASSERT_TRUE(ok_reply.has_value()) << DescribeSendError(err);
  EXPECT_TRUE(ok_reply->ok);
  EXPECT_EQ(ok_reply->message, "ping");
}

TEST_F(ControlSocketTest, HandlerExceptionRepliesInternalErrorAndSurvives) {
  auto path = (dir_ / "control.sock").string();
  ControlSocket server(path, MakeLogger());

  server.Listen([](std::string_view message) -> ControlReply {
    if (message == "boom") throw std::runtime_error("unexpected failure");
    return {true, "alive"};
  });

  int err = 0;
  auto fail_reply = SendRequest(path, "boom", &err);
  ASSERT_TRUE(fail_reply.has_value()) << DescribeSendError(err);
  EXPECT_FALSE(fail_reply->ok);
  EXPECT_EQ(fail_reply->message, "Internal error");

  auto ok_reply = SendRequest(path, "ping", &err);
  ASSERT_TRUE(ok_reply.has_value()) << DescribeSendError(err);
  EXPECT_TRUE(ok_reply->ok);
  EXPECT_EQ(ok_reply->message, "alive");
}

TEST_F(ControlSocketTest, StopListeningWakesImmediatelyWithIdleClient) {
  auto path = (dir_ / "control.sock").string();
  ControlSocket::Options opts;
  opts.peer_idle_timeout_ms = 5000;
  ControlSocket server(path, MakeLogger(), opts);
  server.Listen([](std::string_view) -> ControlReply { return {true, "ok"}; });

  UniqueFd idle = ConnectSeqpacket(path);
  ASSERT_TRUE(idle);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  auto start = std::chrono::steady_clock::now();
  server.StopListening();
  auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_LT(elapsed, std::chrono::milliseconds(500));
}

TEST_F(ControlSocketTest, StopListeningDeliversInFlightReply) {
  auto path = (dir_ / "control.sock").string();
  ControlSocket server(path, MakeLogger());

  std::promise<void> in_handler;
  std::promise<void> release_handler;
  auto release_future = release_handler.get_future();

  server.Listen([&](std::string_view) -> ControlReply {
    in_handler.set_value();
    release_future.wait();
    return {true, "completed"};
  });

  int err = 0;
  std::optional<ControlReply> reply;
  std::thread client([&] { reply = SendRequest(path, "ping", &err); });

  ASSERT_EQ(in_handler.get_future().wait_for(std::chrono::seconds(2)),
            std::future_status::ready);
  std::thread stopper([&] { server.StopListening(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  release_handler.set_value();

  client.join();
  stopper.join();
  ASSERT_TRUE(reply.has_value()) << DescribeSendError(err);
  EXPECT_TRUE(reply->ok);
  EXPECT_EQ(reply->message, "completed");
}

TEST_F(ControlSocketTest, ClientTimesOutWaitingForReply) {
  auto path = (dir_ / "silent.sock").string();
  UniqueFd listener(::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0));
  ASSERT_TRUE(listener);
  socklen_t len = 0;
  sockaddr_un addr = MakeUnixAddr(path, &len);
  ASSERT_EQ(::bind(listener.get(), reinterpret_cast<sockaddr*>(&addr), len), 0);
  ASSERT_EQ(::listen(listener.get(), 4), 0);

  ClientOptions opts;
  opts.timeout_ms = 100;
  int err = 0;
  auto start = std::chrono::steady_clock::now();
  EXPECT_FALSE(SendRequest(path, "dump_end", &err, opts));
  auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_EQ(err, ETIMEDOUT);
  EXPECT_LT(elapsed, std::chrono::seconds(2));
}

TEST_F(ControlSocketTest, ServerDropsIdleClientAfterTimeout) {
  auto path = (dir_ / "control.sock").string();
  ControlSocket::Options opts;
  opts.peer_idle_timeout_ms = 100;
  ControlSocket server(path, MakeLogger(), opts);
  server.Listen(
      [](std::string_view) -> ControlReply { return {true, "unblocked"}; });

  UniqueFd idle = ConnectSeqpacket(path);
  ASSERT_TRUE(idle);

  ClientOptions client_opts;
  client_opts.timeout_ms = 2000;
  int err = 0;
  auto reply = SendRequest(path, "ping", &err, client_opts);
  ASSERT_TRUE(reply.has_value()) << DescribeSendError(err);
  EXPECT_TRUE(reply->ok);
  EXPECT_EQ(reply->message, "unblocked");
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

TEST_F(ControlSocketTest, CliPrefersSystemSocketOverXdg) {
  auto system_socket = dir_ / "system" / "control.sock";
  std::filesystem::create_directory(system_socket.parent_path());
  auto xdg = dir_ / "xdg";
  std::filesystem::create_directory(xdg);
  setenv(kXdgRuntimeDirEnv, xdg.c_str(), 1);
  EXPECT_EQ(ResolveControlSocketPath(ControlPathSide::kCli, system_socket),
            system_socket);
}

TEST_F(ControlSocketTest, CliFallsBackToSystemSocket) {
  auto system_socket = dir_ / "missing-system" / "control.sock";
  EXPECT_EQ(ResolveControlSocketPath(ControlPathSide::kCli, system_socket),
            system_socket);
}

TEST_F(ControlSocketTest, CliPathUsesDaemonPrecedence) {
  auto system_socket = dir_ / "missing-system" / "control.sock";
  auto xdg = dir_ / "xdg";
  std::filesystem::create_directory(xdg);
  setenv(kXdgRuntimeDirEnv, xdg.c_str(), 1);
  EXPECT_EQ(ResolveControlSocketPath(ControlPathSide::kCli, system_socket),
            xdg / "volta" / "control.sock");
  EXPECT_FALSE(std::filesystem::exists(xdg / "volta"));

  auto runtime = dir_ / "runtime";
  std::filesystem::create_directory(runtime);
  setenv(kRuntimeDirectoryEnv, runtime.c_str(), 1);
  EXPECT_EQ(ResolveControlSocketPath(ControlPathSide::kCli, system_socket),
            runtime / "control.sock");

  auto override_path = dir_ / "override" / "control.sock";
  setenv(kControlSocketEnv, override_path.c_str(), 1);
  EXPECT_EQ(ResolveControlSocketPath(ControlPathSide::kCli, system_socket),
            override_path);

  unsetenv(kControlSocketEnv);
  auto missing_runtime = dir_ / "missing-runtime";
  setenv(kRuntimeDirectoryEnv, missing_runtime.c_str(), 1);
  EXPECT_THROW(ResolveControlSocketPath(ControlPathSide::kCli, system_socket),
               ControlPathError);
}

}  // namespace
}  // namespace control
}  // namespace agent
}  // namespace volta
