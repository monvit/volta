#include "exporter.h"

#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>

#include "gtest/gtest.h"

namespace volta {
namespace agent {
namespace {

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

int CountCsvFiles(const std::filesystem::path& dir) {
  int count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() == ".csv") ++count;
  }
  return count;
}

class ExporterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = std::filesystem::temp_directory_path() /
           ("volta-exporter-" + std::to_string(getpid()) + "-" +
            std::to_string(counter_++));
    std::filesystem::create_directories(dir_);
    cfg_.dump_dir = dir_;
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::permissions(dir_, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::add, ec);
    std::filesystem::remove_all(dir_, ec);
  }

  std::filesystem::path dir_;
  config::Config cfg_;

 private:
  inline static int counter_ = 0;
};

TEST_F(ExporterTest, InactiveStateDoesNotCreateFile) {
  Exporter exporter(cfg_);
  EXPECT_FALSE(exporter.IsActive());

  Metric metric;
  metric.type = MetricType::METRIC_TYPE_CPU_UTILIZATION;
  metric.timestamp = 1;
  metric.value = 1.0;
  exporter.Dump({metric});

  EXPECT_FALSE(exporter.IsActive());
  EXPECT_TRUE(std::filesystem::is_empty(dir_));
}

TEST_F(ExporterTest, StartDumpWritesHeaderAndAppendsRow) {
  Exporter exporter(cfg_);
  const auto started = exporter.StartDump(std::nullopt);
  ASSERT_EQ(started.status, DumpRequestStatus::kStarted);
  EXPECT_TRUE(exporter.IsActive());
  EXPECT_EQ(started.path.parent_path(), dir_);
  EXPECT_TRUE(started.path.filename().string().starts_with("dump_"));
  EXPECT_EQ(started.path.extension(), ".csv");
  EXPECT_EQ(ReadFile(started.path), "metric_type;device_id;timestamp;value\n");

  Metric metric;
  metric.type = MetricType::METRIC_TYPE_CPU_UTILIZATION;
  metric.timestamp = 123;
  metric.value = 1.5;
  exporter.Dump({metric});

  EXPECT_EQ(ReadFile(started.path),
            "metric_type;device_id;timestamp;value\n"
            "METRIC_TYPE_CPU_UTILIZATION;system;123;1.5\n");
}

TEST_F(ExporterTest, StartDumpWhileActivePreservesOriginalFile) {
  const auto other = dir_ / "other";
  std::filesystem::create_directory(other);

  Exporter exporter(cfg_);
  const auto first = exporter.StartDump(std::nullopt);
  ASSERT_EQ(first.status, DumpRequestStatus::kStarted);
  const auto before = ReadFile(first.path);

  const auto second = exporter.StartDump(other);
  EXPECT_EQ(second.status, DumpRequestStatus::kAlreadyRunning);
  EXPECT_EQ(second.path, first.path);
  EXPECT_TRUE(exporter.IsActive());
  EXPECT_EQ(ReadFile(first.path), before);
  EXPECT_EQ(CountCsvFiles(dir_), 1);
  EXPECT_TRUE(std::filesystem::is_empty(other));

  const auto third = exporter.StartDump(dir_ / "missing");
  EXPECT_EQ(third.status, DumpRequestStatus::kAlreadyRunning);
  EXPECT_EQ(third.path, first.path);
  EXPECT_EQ(CountCsvFiles(dir_), 1);
}

TEST_F(ExporterTest, StartDumpMissingConfiguredDirectory) {
  const auto missing = dir_ / "missing-configured";
  cfg_.dump_dir = missing;
  Exporter exporter(cfg_);

  const auto result = exporter.StartDump(std::nullopt);
  EXPECT_EQ(result.status, DumpRequestStatus::kDirectoryMissing);
  EXPECT_EQ(result.path, missing);
  EXPECT_FALSE(exporter.IsActive());
  EXPECT_FALSE(std::filesystem::exists(missing));
}

TEST_F(ExporterTest, StartDumpMissingOverrideDirectory) {
  const auto missing = dir_ / "missing-override";
  Exporter exporter(cfg_);

  const auto result = exporter.StartDump(missing);
  EXPECT_EQ(result.status, DumpRequestStatus::kDirectoryMissing);
  EXPECT_EQ(result.path, missing);
  EXPECT_FALSE(exporter.IsActive());
  EXPECT_FALSE(std::filesystem::exists(missing));
  EXPECT_TRUE(std::filesystem::is_empty(dir_));
}

TEST_F(ExporterTest, StartDumpEmptyDirectory) {
  cfg_.dump_dir.clear();
  Exporter configured(cfg_);
  const auto configured_result = configured.StartDump(std::nullopt);
  EXPECT_EQ(configured_result.status, DumpRequestStatus::kDirectoryMissing);
  EXPECT_TRUE(configured_result.path.empty());
  EXPECT_FALSE(configured.IsActive());

  cfg_.dump_dir = dir_;
  Exporter overridden(cfg_);
  const auto override_result = overridden.StartDump(std::filesystem::path{});
  EXPECT_EQ(override_result.status, DumpRequestStatus::kDirectoryMissing);
  EXPECT_TRUE(override_result.path.empty());
  EXPECT_FALSE(overridden.IsActive());
  EXPECT_TRUE(std::filesystem::is_empty(dir_));
}

TEST_F(ExporterTest, StartDumpRegularFilePath) {
  const auto file = dir_ / "not-a-directory";
  {
    std::ofstream out(file);
    out << "regular file\n";
  }
  Exporter exporter(cfg_);

  const auto result = exporter.StartDump(file);
  EXPECT_EQ(result.status, DumpRequestStatus::kNotADirectory);
  EXPECT_EQ(result.path, file);
  EXPECT_FALSE(exporter.IsActive());
  EXPECT_EQ(ReadFile(file), "regular file\n");
  EXPECT_EQ(CountCsvFiles(dir_), 0);
}

TEST_F(ExporterTest, StartDumpUnwritableDirectory) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "root bypasses directory write permissions";
  }

  std::filesystem::permissions(dir_, std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_exec);

  Exporter exporter(cfg_);
  const auto result = exporter.StartDump(std::nullopt);
  EXPECT_EQ(result.status, DumpRequestStatus::kOpenFailed);
  EXPECT_EQ(result.error, EACCES);
  EXPECT_EQ(result.path.parent_path(), dir_);
  EXPECT_TRUE(result.path.filename().string().starts_with("dump_"));
  EXPECT_FALSE(exporter.IsActive());
  EXPECT_TRUE(std::filesystem::is_empty(dir_));
}

TEST_F(ExporterTest, StartDumpHeaderWriteFailureRemovesPartialFile) {
  const pid_t pid = ::fork();
  ASSERT_GE(pid, 0);
  if (pid == 0) {
    std::signal(SIGXFSZ, SIG_IGN);
    rlimit zero_fsize{0, 0};
    if (::setrlimit(RLIMIT_FSIZE, &zero_fsize) != 0) _exit(2);
    Exporter exporter(cfg_);
    const auto result = exporter.StartDump(std::nullopt);
    if (result.status != DumpRequestStatus::kOpenFailed) _exit(10);
    if (result.error != EFBIG) _exit(11);
    if (exporter.IsActive()) _exit(12);
    _exit(0);
  }

  int status = 0;
  ASSERT_EQ(::waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
  EXPECT_EQ(CountCsvFiles(dir_), 0);
}

TEST_F(ExporterTest, EndDumpWhenActiveReturnsClosedPath) {
  Exporter exporter(cfg_);
  const auto started = exporter.StartDump(std::nullopt);
  ASSERT_EQ(started.status, DumpRequestStatus::kStarted);

  const auto stopped = exporter.EndDump();
  ASSERT_TRUE(stopped.has_value());
  EXPECT_EQ(*stopped, started.path);
  EXPECT_FALSE(exporter.IsActive());
  EXPECT_TRUE(std::filesystem::is_regular_file(*stopped));
  EXPECT_EQ(ReadFile(*stopped), "metric_type;device_id;timestamp;value\n");

  Metric metric;
  metric.timestamp = 9;
  metric.value = 9.0;
  exporter.Dump({metric});
  EXPECT_EQ(ReadFile(*stopped), "metric_type;device_id;timestamp;value\n");
  EXPECT_FALSE(exporter.EndDump().has_value());
}

TEST_F(ExporterTest, EndDumpWhenIdleReturnsNullopt) {
  Exporter exporter(cfg_);
  EXPECT_FALSE(exporter.EndDump().has_value());
  EXPECT_FALSE(exporter.IsActive());
  EXPECT_TRUE(std::filesystem::is_empty(dir_));
}

TEST(ExporterReplyTest, FormatsResultsAsControlReplies) {
  const auto started = FormatDumpReply(DumpRequestResult{
      .status = DumpRequestStatus::kStarted,
      .path = "/var/lib/volta/dump.csv",
  });
  EXPECT_TRUE(started.ok);
  EXPECT_EQ(started.message, "Dump started: /var/lib/volta/dump.csv");

  const auto already = FormatDumpReply(DumpRequestResult{
      .status = DumpRequestStatus::kAlreadyRunning,
      .path = "/tmp/active.csv",
  });
  EXPECT_FALSE(already.ok);
  EXPECT_EQ(already.message, "Dump already running: /tmp/active.csv");

  const auto missing = FormatDumpReply(DumpRequestResult{
      .status = DumpRequestStatus::kDirectoryMissing,
      .path = "/var/lib/volta/dumps",
  });
  EXPECT_FALSE(missing.ok);
  EXPECT_EQ(missing.message,
            "Dump directory does not exist: /var/lib/volta/dumps");

  const auto empty = FormatDumpReply(DumpRequestResult{
      .status = DumpRequestStatus::kDirectoryMissing,
  });
  EXPECT_FALSE(empty.ok);
  EXPECT_EQ(empty.message, "Dump directory not configured");

  const auto not_dir = FormatDumpReply(DumpRequestResult{
      .status = DumpRequestStatus::kNotADirectory,
      .path = "/tmp/regular-file",
  });
  EXPECT_FALSE(not_dir.ok);
  EXPECT_EQ(not_dir.message, "Dump path is not a directory: /tmp/regular-file");

  const auto failed = FormatDumpReply(DumpRequestResult{
      .status = DumpRequestStatus::kOpenFailed,
      .path = "/tmp/dir/dump.csv",
      .error = EACCES,
  });
  EXPECT_FALSE(failed.ok);
  EXPECT_EQ(failed.message, "Cannot open dump file: /tmp/dir/dump.csv: " +
                                std::string(std::strerror(EACCES)));

  const auto stopped =
      FormatDumpReply(std::optional<std::filesystem::path>{"/tmp/a.csv"});
  EXPECT_TRUE(stopped.ok);
  EXPECT_EQ(stopped.message, "Dump stopped: /tmp/a.csv");

  const auto idle = FormatDumpReply(std::optional<std::filesystem::path>{});
  EXPECT_TRUE(idle.ok);
  EXPECT_EQ(idle.message, "No dump was running");
}

}  // namespace
}  // namespace agent
}  // namespace volta
