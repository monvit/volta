#ifndef VOLTA_AGENT_SRC_COLLECTORS_NVML_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_NVML_COLLECTOR_H_

#include <nvml.h>

#include "collectors/collector.h"

namespace volta {
namespace agent {
namespace collectors {

class NvmlCollector : public RegisteredCollector<NvmlCollector> {
 public:
  NvmlCollector();
  ~NvmlCollector() override;

  NvmlCollector(NvmlCollector&&) = default;
  NvmlCollector& operator=(NvmlCollector&&) = default;

  bool Init() override;
  std::vector<Metric> Collect() override;
  bool IsSupported() override;
  std::vector<v1::MetricType> Satisfiable() override;

 private:
  nvmlDevice_t device_handle_;
  bool initialized_ = false;
};

}  // namespace collectors
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_COLLECTORS_NVML_COLLECTOR_H_
