#include "platform/platform_detector.h"

#include <unistd.h>
#include <cstring>
#include <fstream>
#include <regex>
#include <iostream>

#ifdef HAVE_NVML
#include <nvml.h>
#endif

namespace volta {
namespace agent {
namespace platform {

static std::string CpuVendorToString(CpuVendor v) {
    switch(v) {
        case CpuVendor::INTEL: return "Intel";
        case CpuVendor::AMD:   return "AMD";
        default:               return "Unknown";
    }
}

static std::string GpuVendorToString(GpuVendor v) {
    switch(v) {
        case GpuVendor::NVIDIA: return "NVIDIA";
        case GpuVendor::AMD:    return "AMD";
        case GpuVendor::INTEL:  return "Intel";
        default:                return "Unknown";
    }
}

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

void PlatformDetector::PrintDetectedInfo(const HardwareInfo& info) {
    std::cout << "[" << typeid(*this).name() << "] Hardware Detection Result:\n";
    std::cout << "  > Host:  " << info.hostname << "\n";
    std::cout << "  > OS:    " << info.os_version << "\n";
    std::cout << "  > CPU:   " << CpuVendorToString(info.cpu.vendor) << "\n"
              << "  | Model: " << info.cpu.model_name << "\n";
    
    if (info.gpus.empty()) {
        std::cout << "  > GPU:  None detected (or NVML disabled)\n";
    } else {
        for (const auto& gpu : info.gpus) {
            std::cout << "  > GPU:   " << GpuVendorToString(gpu.vendor) << "\n" 
                      << "  | Model: " << gpu.model_name << "\n";
        }
    }
    std::cout << "--------------------------------------------------\n";
    std::cout.flush();
    std::cin.get();
}

std::string PlatformDetector::DetectOS() {
    std::ifstream file("/etc/os-release");
    
    if (!file.is_open()) {
        return "Linux (Unknown - Cannot open /etc/os-release)";
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("PRETTY_NAME=") == 0) {
            std::string value = line.substr(12);

            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            
            return value;
        }
    }

    return "Linux (Unknown - Tag not found)";
}

} // namespace platform
} // namespace agent
} // namespace volta
