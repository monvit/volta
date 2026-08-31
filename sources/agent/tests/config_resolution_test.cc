#include "config/config_resolution.h"

#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "gtest/gtest.h"

namespace volta {
namespace agent {
namespace config {
namespace {

class TempDir {
 public:
  explicit TempDir(const std::string& prefix)
      : path_(std::filesystem::temp_directory_path() /
              (prefix + "-" + std::to_string(++counter_))) {
    std::filesystem::create_directories(path_);
  }
  ~TempDir() { std::filesystem::remove_all(path_); }
  const std::filesystem::path& path() const { return path_; }

 private:
  inline static int counter_ = 0;
  std::filesystem::path path_;
};

std::filesystem::path WriteFile(const std::filesystem::path& dir,
                                const std::string& name,
                                const std::string& body) {
  auto p = dir / name;
  std::ofstream out(p);
  out << body;
  return p;
}

class RestoredPermissions {
 public:
  RestoredPermissions(const std::filesystem::path& path,
                      std::filesystem::perms restore_perms)
      : path_(path), restore_perms_(restore_perms) {}
  ~RestoredPermissions() {
    std::error_code ec;
    std::filesystem::permissions(path_, restore_perms_,
                                 std::filesystem::perm_options::replace, ec);
  }

 private:
  std::filesystem::path path_;
  std::filesystem::perms restore_perms_;
};

class RestoredCurrentPath {
 public:
  explicit RestoredCurrentPath(const std::filesystem::path& path)
      : original_(std::filesystem::current_path()) {
    std::filesystem::current_path(path);
  }
  ~RestoredCurrentPath() {
    std::error_code ec;
    std::filesystem::current_path(original_, ec);
  }

 private:
  std::filesystem::path original_;
};

class ConfigResolutionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (const char* v = std::getenv(kConfigEnvVar)) {
      saved_env_ = v;
    }
    unsetenv(kConfigEnvVar);
  }

  void TearDown() override {
    if (saved_env_) {
      setenv(kConfigEnvVar, saved_env_->c_str(), 1);
    } else {
      unsetenv(kConfigEnvVar);
    }
  }

  void SetConfigEnv(const std::string& value) {
    ASSERT_EQ(setenv(kConfigEnvVar, value.c_str(), 1), 0);
  }

  TempDir tmp_{"volta-config-test"};
  std::filesystem::path system_path_{tmp_.path() / "etc-volta-agent.conf"};
  std::optional<std::string> saved_env_;
};

TEST_F(ConfigResolutionTest, ParseCli) {
  const char* argv1[] = {"prog", "--config", "/a.conf"};
  auto o1 = ParseCli(3, argv1);
  ASSERT_TRUE(o1.has_value());
  EXPECT_EQ(*o1->config_flag, "/a.conf");

  const char* argv2[] = {"prog", "--config=/b.conf"};
  auto o2 = ParseCli(2, argv2);
  ASSERT_TRUE(o2.has_value());
  EXPECT_EQ(*o2->config_flag, "/b.conf");

  const char* argv3[] = {"prog", "--config"};
  EXPECT_FALSE(ParseCli(2, argv3).has_value());

  const char* argv4[] = {"prog", "--help"};
  auto o4 = ParseCli(2, argv4);
  ASSERT_TRUE(o4.has_value());
  EXPECT_TRUE(o4->show_help);

  const char* argv5[] = {"prog", "--unknown"};
  EXPECT_FALSE(ParseCli(2, argv5).has_value());
}

TEST_F(ConfigResolutionTest, ResolutionPrecedence) {
  auto cli_file = WriteFile(tmp_.path(), "cli.conf", "from=cli\n");
  auto env_file = WriteFile(tmp_.path(), "env.conf", "from=env\n");
  WriteFile(system_path_.parent_path(), system_path_.filename().string(),
            "from=system\n");
  SetConfigEnv(env_file.string());

  CliOptions cli;
  cli.config_flag = cli_file;
  auto resolved = ResolveConfig(cli, system_path_);
  ASSERT_TRUE(resolved.path.has_value());
  EXPECT_EQ(*resolved.path, cli_file);
  EXPECT_EQ(resolved.source, ConfigSource::kCli);

  unsetenv(kConfigEnvVar);
  resolved = ResolveConfig(CliOptions{}, system_path_);
  ASSERT_TRUE(resolved.path.has_value());
  EXPECT_EQ(*resolved.path, system_path_);
  EXPECT_EQ(resolved.source, ConfigSource::kSystem);

  SetConfigEnv(env_file.string());
  resolved = ResolveConfig(CliOptions{}, system_path_);
  ASSERT_TRUE(resolved.path.has_value());
  EXPECT_EQ(*resolved.path, env_file);
  EXPECT_EQ(resolved.source, ConfigSource::kEnvironment);
}

TEST_F(ConfigResolutionTest, DefaultsWhenNothingPresent) {
  std::error_code ec;
  std::filesystem::remove(system_path_, ec);

  auto resolved = ResolveConfig(CliOptions{}, system_path_);
  EXPECT_FALSE(resolved.path.has_value());
  EXPECT_EQ(resolved.source, ConfigSource::kBuiltinDefaults);
}

TEST_F(ConfigResolutionTest, InvalidExplicitSourcesFail) {
  CliOptions cli;
  cli.config_flag = tmp_.path() / "missing.conf";
  try {
    ResolveConfig(cli, system_path_);
    FAIL();
  } catch (const ResolutionError& e) {
    EXPECT_EQ(e.errno_code, ENOENT);
  }

  SetConfigEnv((tmp_.path() / "missing.conf").string());
  try {
    ResolveConfig(CliOptions{}, system_path_);
    FAIL();
  } catch (const ResolutionError& e) {
    EXPECT_EQ(e.errno_code, ENOENT);
  }

  SetConfigEnv("");
  try {
    ResolveConfig(CliOptions{}, system_path_);
    FAIL();
  } catch (const ResolutionError& e) {
    EXPECT_EQ(e.errno_code, EINVAL);
  }
}

TEST_F(ConfigResolutionTest, UnreadableExplicit) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "permission checks are bypassed by root";
  }

  auto p = WriteFile(tmp_.path(), "secret.conf", "x\n");
  std::filesystem::permissions(p, std::filesystem::perms::none,
                               std::filesystem::perm_options::replace);
  RestoredPermissions restore{p, std::filesystem::perms::owner_all};

  CliOptions cli;
  cli.config_flag = p;
  EXPECT_THROW(ResolveConfig(cli, system_path_), ResolutionError);
}

TEST_F(ConfigResolutionTest, UnreadableSystemPathDoesNotFallback) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "permission checks are bypassed by root";
  }

  auto p = WriteFile(system_path_.parent_path(),
                     system_path_.filename().string(), "from=system\n");
  std::filesystem::permissions(p, std::filesystem::perms::none,
                               std::filesystem::perm_options::replace);
  RestoredPermissions restore{p, std::filesystem::perms::owner_all};

  EXPECT_THROW(ResolveConfig(CliOptions{}, system_path_), ResolutionError);
}

TEST_F(ConfigResolutionTest, RelativeConfigPath) {
  WriteFile(tmp_.path(), "rel.conf", "relative=yes\n");
  RestoredCurrentPath cwd(tmp_.path());

  CliOptions cli;
  cli.config_flag = std::filesystem::path("rel.conf");
  auto resolved = ResolveConfig(cli, system_path_);
  ASSERT_TRUE(resolved.path.has_value());
  EXPECT_EQ(*resolved.path, std::filesystem::path("rel.conf"));
  EXPECT_EQ(resolved.source, ConfigSource::kCli);
}

}  // namespace
}  // namespace config
}  // namespace agent
}  // namespace volta
