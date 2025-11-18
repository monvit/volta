#ifndef VOLTA_AGENT_PLATFORM_GPU_INFO_H_
#define VOLTA_AGENT_PLATFORM_GPU_INFO_H_

#include <string>

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
    // Crucial determinant for Collector selection
    GpuVendor vendor = GpuVendor::UNKNOWN;
    
    // Optional, can be derived from vendor enum
    std::string vendor_name;
    
    // Whole model name a.e. "NVIDIA GeForce RTX 3080"
    std::string model_name;
    
    // Unique identifier (Domain:Bus:Device.Function)
    // Allows to differentiate multiple GPUs in one system
    std::string pci_address;

    // Driver version string
    std::string driver_version;
    
    // VRAM size in bytes
    u_int64_t vram_total_bytes = 0;

    // TDP or Power Limit in Watts
    double power_limit_watts = 0.0;
};

} // namespace platform
} // namespace agent
} // namespace volta

#endif  // VOLTA_AGENT_PLATFORM_GPU_INFO_H_