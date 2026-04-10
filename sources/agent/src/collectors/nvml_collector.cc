#include "collectors/nvml_collector.h"

#include <chrono>
#include <iostream>

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
    std::cerr << "Failed to initialize NVML: " << nvmlErrorString(result)
              << std::endl;
    return false;
  }

  result = nvmlDeviceGetHandleByIndex(0, &device_handle_);
  if (result != NVML_SUCCESS) {
    std::cerr << "Failed to get device handle: " << nvmlErrorString(result)
              << std::endl;
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
    metrics.push_back({"gpu_0_power_watts",
                       static_cast<double>(power_mw) / 1000.0,  // mW -> W
                       now});
  }

  unsigned int temp_c = 0;
  result =
      nvmlDeviceGetTemperature(device_handle_, NVML_TEMPERATURE_GPU, &temp_c);
  if (result == NVML_SUCCESS) {
    metrics.push_back({"gpu_0_temp_celsius", static_cast<double>(temp_c), now});
  }

  nvmlUtilization_t utilization;
  result = nvmlDeviceGetUtilizationRates(device_handle_, &utilization);

  if (result == NVML_SUCCESS) {
    metrics.push_back({"gpu_0_utilization_percent",
                       static_cast<double>(utilization.gpu), now});

    metrics.push_back({"gpu_0_memory_activity_percent",
                       static_cast<double>(utilization.memory), now});
  }

  nvmlMemory_t memory;
  result = nvmlDeviceGetMemoryInfo(device_handle_, &memory);

  if (result == NVML_SUCCESS) {
    metrics.push_back(
        {"gpu_0_memory_total_bytes", static_cast<double>(memory.total), now});
    metrics.push_back(
        {"gpu_0_memory_used_bytes", static_cast<double>(memory.used), now});
    double used_percent = (double)memory.used / memory.total * 100.0;
    metrics.push_back({"gpu_0_memory_used_percent", used_percent, now});
  }

  return metrics;
}

}  // namespace collectors
}  // namespace agent
}  // namespace volta
