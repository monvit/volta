#ifndef VOLTA_AGENT_PLATFORM_HARDWARE_INFO_H_
#define VOLTA_AGENT_PLATFORM_HARDWARE_INFO_H_

#include <cstdint>
#include <string>
#include <vector>

#include "platform/cpu_info.h"
#include "platform/gpu_info.h"

namespace volta {
namespace agent {
namespace platform {

struct HardwareInfo {
  std::string hostname;
  std::string os_version;
  std::string kernel_version;

  uint64_t total_ram_bytes = 0;

  CpuInfo cpu;
  std::vector<GpuInfo> gpus;
};

}  // namespace platform
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_PLATFORM_HARDWARE_INFO_H_
