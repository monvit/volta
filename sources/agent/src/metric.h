#ifndef VOLTA_AGENT_SRC_METRIC_H
#define VOLTA_AGENT_SRC_METRIC_H

#include <cstdint>
#include <optional>
#include <variant>

#include "volta.pb.h"

namespace volta {
namespace agent {

using DeviceId = std::variant<GpuID, CpuID, NetInterfaceID, DiskID>;

struct Metric {
  MetricType type = MetricType::METRIC_TYPE_UNSPECIFIED;
  std::optional<DeviceId> devId;
  double value = 0.0;
  int64_t timestamp = 0;

  bool HasDevice() const { return devId.has_value(); }
};

}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_METRIC_H
