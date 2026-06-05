#ifndef VOLTA_AGENT_SRC_SCHEDULER_H_
#define VOLTA_AGENT_SRC_SCHEDULER_H_

#include <memory>
#include <string>
#include <vector>

#include "buffer.h"
#include "collectors/collector.h"
#include "config/config.h"
#include "metric.h"

namespace volta {
namespace agent {

class Scheduler {
 public:
  explicit Scheduler(const config::Config& config,
                     std::vector<collectors::Collector*>&& collectors,
                     std::shared_ptr<MetricsBuffer> buffer);

  void Run();

 private:
  void PrintDashboard();
  static std::string DescribeKey(const BufferKey& key);

  std::vector<collectors::Collector*> collectors_;
  const config::Config& config_;
  std::shared_ptr<MetricsBuffer> buffer_;
};

}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_SCHEDULER_H_
