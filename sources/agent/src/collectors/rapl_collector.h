#ifndef VOLTA_AGENT_SRC_COLLECTORS_RAPL_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_RAPL_COLLECTOR_H_

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "collectors/collector.h"

namespace volta {
namespace agent {
namespace collectors {

class RaplCollector final : public RegisteredCollector<RaplCollector> {
 public:
  RaplCollector();
  RaplCollector(const RaplCollector&) = delete;
  RaplCollector& operator=(const RaplCollector&) = delete;
  bool Init() override;
  std::vector<Metric> Collect() override;
  bool IsSupported() override;
  std::vector<MetricType> Satisfiable() override;
  void SetRequestedMetrics(const std::vector<MetricType>& metrics) override;
  ~RaplCollector();

 private:
  struct PowercapZone {
    std::filesystem::path path;
    int energy_fd = -1;
    uint32_t socket_index = 0;
    uint64_t max_energy_range_uj = 0;
    uint64_t last_energy_uj = 0;
    std::chrono::steady_clock::time_point last_sample_time{};
  };

  bool OpenPowercap();
  void ClosePowercap();

  std::vector<MetricType> requested_metrics_;
  bool initialized_ = false;
  std::vector<PowercapZone> zones_;
};

}  // namespace collectors
}  // namespace agent
}  // namespace volta
#endif
