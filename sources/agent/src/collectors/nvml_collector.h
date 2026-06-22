#ifndef VOLTA_AGENT_SRC_COLLECTORS_NVML_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_NVML_COLLECTOR_H_

#include <vector>

#include "collectors/collector.h"
#include "platform/nvml_loader.h"

namespace volta {
namespace agent {
namespace collectors {

class NvmlCollector final : public RegisteredCollector<NvmlCollector> {
 public:
  NvmlCollector();
  ~NvmlCollector() override;

  NvmlCollector(NvmlCollector&&) = default;
  NvmlCollector& operator=(NvmlCollector&&) = default;

  bool Init() override;
  std::vector<Metric> Collect() override;
  bool IsSupported() override;
  std::vector<MetricType> Satisfiable() override;
  void SetRequestedMetrics(const std::vector<MetricType>& metrics) override;

 private:
  struct DeviceInfo {
    nvmlDevice_t handle;
    GpuID id;
  };

  std::vector<MetricType> requested_metrics_;
  std::vector<DeviceInfo> devices_;

  const platform::NvmlApi* nvml_ = nullptr;
  bool initialized_ = false;
};

}  // namespace collectors
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_COLLECTORS_NVML_COLLECTOR_H_
