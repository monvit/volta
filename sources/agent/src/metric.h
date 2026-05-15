#ifndef VOLTA_AGENT_SRC_METRIC_H
#define VOLTA_AGENT_SRC_METRIC_H

#include <cstdint>
#include <optional>
#include <string>

#include "volta.pb.h"

namespace volta {
namespace agent {

struct DeviceId {
  std::optional<std::string>
      uuid;  // device UUID from NVML/ROCm, disk serial, etc.
  std::optional<std::string> name;  // Human-readable: "sda", "eth0", "GPU 0"
  std::optional<uint32_t> index;    // 0-based index (GPU slot, CPU core, etc.)
  std::optional<std::string> vendor;  // "nvidia" | "amd" | "intel"
};

struct Metric {
  v1::MetricType type;
  DeviceId devId;
  double value;
  int64_t timestamp;
};

}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_METRIC_H
