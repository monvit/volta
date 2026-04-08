#ifndef VOLTA_AGENT_SRC_COLLECTORS_NVML_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_NVML_COLLECTOR_H_

#include <nvml.h>

#include "collectors/collector.h"

namespace volta {
namespace agent {
namespace collectors {

class NvmlCollector : public Collector {
 public:
  NvmlCollector();
  ~NvmlCollector() override;

  bool Init() override;
  std::vector<Metric> Collect() override;

 private:
  nvmlDevice_t device_handle_;
  bool initialized_ = false;
};

}  // namespace collectors
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_COLLECTORS_NVML_COLLECTOR_H_
