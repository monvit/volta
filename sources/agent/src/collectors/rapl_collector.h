#ifndef VOLTA_AGENT_SRC_COLLECTORS_RAPL_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_RAPL_COLLECTOR_H_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "collectors/collector.h"

namespace volta {
namespace agent {
namespace collectors {

class RaplCollector final : public RegisteredCollector<RaplCollector> {
 public:
  bool Init() override;
  std::vector<Metric> Collect() override;
  bool IsSupported() override;
  std::vector<MetricType> Satisfiable() override;
  void SetRequestedMetrics(const std::vector<MetricType>& metrics) override;

 private:
  struct PackageZone {
    std::filesystem::path path;
    std::string name;
    int socket_index = 0;
    uint64_t max_energy_range_uj = 0;
  };

  static bool IsPackageName(const std::string& name);
  static std::optional<int> ParseSocketIndex(const std::string& name);
  static uint64_t DeltaEnergyUj(uint64_t now, uint64_t last,
                                uint64_t max_range_uj);

  std::vector<PackageZone> DiscoverPackages() const;
  bool ReadU64File(const std::filesystem::path& path, uint64_t* out) const;
  bool ReadNameFile(const std::filesystem::path& path, std::string* out) const;

  std::vector<MetricType> requested_metrics_;
  bool initialized_ = false;
  PackageZone active_;
  uint64_t last_energy_uj_ = 0;
};

}  // namespace collectors
}  // namespace agent
}  // namespace volta
#endif
