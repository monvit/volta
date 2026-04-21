#ifndef VOLTA_AGENT_SRC_METRIC_H
#define VOLTA_AGENT_SRC_METRIC_H

#include <cstdint>
#include <optional>
#include <string>

namespace volta {
namespace agent {

enum class MetricType : uint16_t {
  // CPU
  CpuPowerPackage,
  CpuPowerCores,
  CpuClockSpeed,
  CpuUtilization,
  CpuTemperature,
  CpuIowait,
  CpuCacheHitRatio,
  CpuActiveProcesses,

  // GPU (vendor-agnostic — vendor goes in DeviceId)
  GpuPower,
  GpuClockSpeed,
  GpuUtilization,
  GpuTemperature,
  GpuVramUsed,
  GpuPcieBandwidth,
  GpuComputeUnitUtilization,  // SM% on NVIDIA, CU% on AMD, EU% on Intel
  GpuSharedMemoryUtilization,
  GpuRegisterUtilization,

  // RAM
  RamPower,
  RamTotal,
  RamAvailable,
  RamUsed,
  RamCached,
  SwapUsed,
  SwapActivity,

  // Disk
  DiskReadThroughput,
  DiskWriteThroughput,
  DiskReadIops,
  DiskWriteIops,
  DiskBusyTime,
  DiskCapacityUsed,

  // Network
  NetBytesReceived,
  NetBytesSent,
  NetPacketsReceived,
  NetPacketsSent,
};

struct DeviceId {
  std::optional<std::string>
      uuid;  // device UUID from NVML/ROCm, disk serial, etc.
  std::optional<std::string> name;  // Human-readable: "sda", "eth0", "GPU 0"
  std::optional<uint32_t> index;    // 0-based index (GPU slot, CPU core, etc.)
  std::optional<std::string> vendor;  // "nvidia" | "amd" | "intel"
};

struct Metric {
  MetricType type;
  DeviceId devId;
  double value;
  int64_t timestamp;
};

}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_METRIC_H
