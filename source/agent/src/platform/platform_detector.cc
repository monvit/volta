#include "platform/platform_detector.h"

#include <unistd.h>
#include <cstring>
#include <fstream>
#include <regex>

#ifdef HAVE_NVML
#include <nvml.h>
#endif

namespace volta {
namespace agent {
namespace platform {

HardwareInfo PlatformDetector::Detect() {
    HardwareInfo info;

    char hostname_buffer[256];
    if (gethostname(hostname_buffer, sizeof(hostname_buffer)) == 0) info.hostname = std::string(hostname_buffer);
    else info.hostname = "unknown-host";

    info.os_version = DetectOS();

    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("vendor_id") != std::string::npos) {
            if (line.find("AuthenticAMD") != std::string::npos) info.cpu.vendor = CpuVendor::AMD;
            else if (line.find("GenuineIntel") != std::string::npos) info.cpu.vendor = CpuVendor::INTEL;
        }
        if (line.find("model name") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                info.cpu.model_name = line.substr(pos + 2); 
            }
            break;
        }
    }

#ifdef HAVE_NVML
    if (nvmlInit() == NVML_SUCCESS) {
        unsigned int device_count = 0;
        nvmlDeviceGetCount(&device_count);

        for (unsigned int i = 0; i < device_count; ++i) {
            nvmlDevice_t device;
            nvmlDeviceGetHandleByIndex(i, &device);

            char name[64];
            if (nvmlDeviceGetName(device, name, sizeof(name)) == NVML_SUCCESS) {
                GpuInfo gpu;
                gpu.vendor = GpuVendor::NVIDIA;
                gpu.model_name = std::string(name);
                info.gpus.push_back(gpu);
            }
        }
        nvmlShutdown();
    }
#endif

    return info;
}

std::string PlatformDetector::DetectOS() {
    std::ifstream file("/etc/os-release");
    std::string line;
    std::regex re("PRETTY_NAME=\"?(.*?)\"?");
    std::smatch match;

    while (std::getline(file, line)) {
        if (line.find("PRETTY_NAME") != std::string::npos) {
            if (std::regex_search(line, match, re) && match.size() > 1) {
                return match.str(1);
            }
        }
    }

    return "Linux (Unknown)";
}

} // namespace platform
} // namespace agent
} // namespace volta
