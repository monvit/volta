#ifndef VOLTA_AGENT_PLATFORM_GPU_INFO_H_
#define VOLTA_AGENT_PLATFORM_GPU_INFO_H_

#include <string>
#include <cstdint>

namespace volta {
namespace agent {
namespace platform {

enum class GpuVendor {
    UNKNOWN,
    NVIDIA,
    AMD,
    INTEL
};

struct GpuInfo {
    GpuVendor vendor = GpuVendor::UNKNOWN;

    std::string model_name;
    
    // Unique identifier (Domain:Bus:Device.Function) to differentiate multiple GPUs in one system
    std::string pci_address;
    std::string driver_version;

    uint64_t vram_total_bytes = 0;
    float power_limit_watts = 0.0;
};

} // namespace platform
} // namespace agent
} // namespace volta

#endif  // VOLTA_AGENT_PLATFORM_GPU_INFO_H_
