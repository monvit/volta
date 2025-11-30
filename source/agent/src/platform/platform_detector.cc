#include "platform/platform_detector.h"

namespace volta {
namespace agent {
namespace platform {

HardwareInfo PlatformDetector::Detect() {
    HardwareInfo info;

    info.os_version = "Fedora Linux 42 (Hardcoded)";
    
    info.cpu.vendor = CpuVendor::AMD;
    info.cpu.model_name = "AMD Ryzen 7 8845HS (Hardcoded)";
    info.cpu.physical_cores = 8;
    info.cpu.has_rapl = true;
    
    GpuInfo my_gpu;
    my_gpu.vendor = GpuVendor::NVIDIA;
    my_gpu.model_name = "RTX 4060 Mobile (Hardcoded)";
    my_gpu.pci_address = "0000:01:00.0";
    
    info.gpus.push_back(my_gpu);

    return info;
}

} // namespace platform
} // namespace agent
} // namespace volta