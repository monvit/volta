#ifndef VOLTA_AGENT_SRC_SCHEDULER_H_
#define VOLTA_AGENT_SRC_SCHEDULER_H_

#include <memory>
#include <vector>

#include "collectors/collector.h"
#include "config/config.h"
#include "metric.h"

namespace volta {
namespace agent {

class Scheduler {
 public:
  explicit Scheduler(
      const config::Config& config,
      std::vector<std::unique_ptr<collectors::Collector>>&& collectors);

  void Run();

 private:
  void PrintDashboard(const std::vector<Metric>& metrics);

  std::vector<std::unique_ptr<collectors::Collector>> collectors_;
  const config::Config& config_;
};

}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_SCHEDULER_H_
