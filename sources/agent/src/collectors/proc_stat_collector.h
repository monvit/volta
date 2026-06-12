#ifndef VOLTA_AGENT_SRC_COLLECTORS_PROC_STAT_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_PROC_STAT_COLLECTOR_H_

#include <fstream>

#include "collectors/collector.h"

namespace volta {
namespace agent {
namespace collectors {

class ProcStatCollector final : public RegisteredCollector<ProcStatCollector> {
 public:
  bool Init() override;
  std::vector<Metric> Collect() override;
  bool IsSupported() override;
  std::vector<MetricType> Satisfiable() override;
  void SetRequestedMetrics(const std::vector<MetricType>& metrics) override;

 private:
  void ReadCpuStats(uint64_t& idle_time, uint64_t& total_time);

  std::vector<MetricType> requested_metrics_;
  uint64_t prev_total_ = 0;
  uint64_t prev_idle_ = 0;
};

}  // namespace collectors
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_COLLECTORS_PROC_STAT_COLLECTOR_H_
