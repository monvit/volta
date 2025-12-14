#ifndef VOLTA_AGENT_SRC_COLLECTORS_AMD_IGPU_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_AMD_IGPU_COLLECTOR_H_

#include <optional>
#include <map>
#include <filesystem>

#include "collector.h"

namespace volta {
namespace agent {
namespace collectors {

class SysfsCollector : public Collector {
public:
  SysfsCollector() = default;
    ~SysfsCollector() override = default;

  bool Init() override;

  std::vector<Metric> Collect() override;

private:
  std::optional<std::filesystem::path> gpu_;
  std::optional<std::filesystem::path> hwmon_;

  long ReadLongFromFile(const std::filesystem::path& full_path);
  void FindHwmonPath();
};

}  // namespace collectors
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_COLLECTORS_AMD_IGPU_COLLECTOR_H_
