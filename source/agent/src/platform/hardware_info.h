#ifndef VOLTA_AGENT_PLATFORM_HARDWARE_INFO_H_
#define VOLTA_AGENT_PLATFORM_HARDWARE_INFO_H_

#include <string>
#include <vector>

#include "agent/src/platform/gpu_info.h"
#include "agent/src/platform/cpu_info.h"

namespace volta {
namespace agent {
namespace platform {

struct HardwareInfo {
    std::string hostname;
    std::string os_version;
    std::string kernel_version;

    int64_t total_ram_bytes = 0;

    CpuInfo cpu;
    std::vector<GpuInfo> gpus;
};

} // namespace platform
} // namespace agent
} // namespace volta

#endif  // VOLTA_AGENT_PLATFORM_HARDWARE_INFO_H_