#include "collectors/nvml_collector.h"

#include <iostream>
#include <chrono>

namespace volta {
namespace agent {
namespace collectors {

NvmlCollector::NvmlCollector() {
    // Initialization moved to Init()
}

NvmlCollector::~NvmlCollector() {
    if (initialized_) {
        nvmlShutdown();
    }
}

bool NvmlCollector::Init() {
    nvmlReturn_t result = nvmlInit();
    if (result != NVML_SUCCESS) {
        std::cerr << "Failed to initialize NVML: " << nvmlErrorString(result) << std::endl;
        return false;
    }

    result = nvmlDeviceGetHandleByIndex(0, &device_handle_);
    if (result != NVML_SUCCESS) {
        std::cerr << "Failed to get device handle: " << nvmlErrorString(result) << std::endl;
        return false;
    }

    initialized_ = true;
    return true;
}

std::vector<Metric> NvmlCollector::Collect() {
    if (!initialized_) return {};

    std::vector<Metric> metrics;
    unsigned int power_mw = 0;
    auto now = std::chrono::system_clock::now().time_since_epoch().count();

    nvmlReturn_t result = nvmlDeviceGetPowerUsage(device_handle_, &power_mw);
    
    if (result == NVML_SUCCESS) {
        metrics.push_back({
            "gpu_0_power_watts",
            static_cast<double>(power_mw) / 1000.0, // mW -> W
            now
        });
    }

    unsigned int temp_c = 0;
    result = nvmlDeviceGetTemperature(device_handle_, NVML_TEMPERATURE_GPU, &temp_c);
    if (result == NVML_SUCCESS) {
        metrics.push_back({
            "gpu_0_temp_celsius",
            static_cast<double>(temp_c),
            now
        });
    }

    return metrics;
}

} // namespace collectors
} // namespace agent
} // namespace volta
