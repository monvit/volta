#ifndef VOLTA_AGENT_SRC_DUMP_H_
#define VOLTA_AGENT_SRC_DUMP_H_

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "config/config.h"
#include "control/control_command.h"
#include "metric.h"

namespace volta {
namespace agent {

enum class DumpRequestStatus {
  kStarted,
  kAlreadyRunning,
  kDirectoryMissing,
  kNotADirectory,
  kOpenFailed,
};

struct DumpRequestResult {
  DumpRequestStatus status = DumpRequestStatus::kOpenFailed;
  std::filesystem::path path;
  int error = 0;
};

control::ControlReply FormatDumpReply(const DumpRequestResult& result);
control::ControlReply FormatDumpReply(
    const std::optional<std::filesystem::path>& stopped);

class Exporter {
 public:
  explicit Exporter(const config::Config& cfg);
  void Dump(const std::vector<Metric>& metric);
  DumpRequestResult StartDump(
      std::optional<std::filesystem::path> dump_dir_overload);
  std::optional<std::filesystem::path> EndDump();
  bool IsActive() const;

 private:
  std::string DescribeDevice(const Metric& m);

  mutable std::mutex mu_;
  std::ofstream dump_file_;
  std::filesystem::path dump_dir_;
  std::filesystem::path active_path_;
};

}  // namespace agent
}  // namespace volta

#endif
