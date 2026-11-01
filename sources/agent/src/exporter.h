#ifndef VOLTA_AGENT_SRC_DUMP_H_
#define VOLTA_AGENT_SRC_DUMP_H_

#include <fstream>
#include <mutex>

#include "config/config.h"
#include "metric.h"

namespace volta {
namespace agent {

class Exporter {
 public:
  explicit Exporter(const config::Config& cfg);
  void Dump(const std::vector<Metric>& metric);
  void StartDump(std::optional<std::filesystem::path> dump_dir_overload);
  void EndDump();
  bool IsActive() const;

 private:
  std::string DescribeDevice(const Metric& m);

  mutable std::mutex mu_;
  std::ofstream dump_file_;
  std::filesystem::path dump_dir_;
  std::chrono::milliseconds dump_start;
  bool is_exporting = false;
};

}  // namespace agent
}  // namespace volta

#endif
