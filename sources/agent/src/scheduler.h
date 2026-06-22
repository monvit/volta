#ifndef VOLTA_AGENT_SRC_SCHEDULER_H_
#define VOLTA_AGENT_SRC_SCHEDULER_H_

#include <csignal>
#include <memory>
#include <string>
#include <vector>

#include "buffer.h"
#include "collectors/collector.h"
#include "config/config.h"
#include "exporter.h"
#include "message_queue.h"
#include "metric.h"

extern volatile sig_atomic_t g_running;

namespace volta {
namespace agent {

class Scheduler {
 public:
  explicit Scheduler(const config::Config& config,
                     std::vector<collectors::Collector*>&& collectors);

  void Run();

 private:
  void PrintDashboard();
  static std::string DescribeKey(const BufferKey& key);

  std::vector<collectors::Collector*> collectors_;
  const config::Config& config_;
  MetricsBuffer buffer_;
  MessageQueue ms_queue_;
  Exporter exporter_;
};

}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_SCHEDULER_H_
