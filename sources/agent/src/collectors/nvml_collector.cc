#include "collectors/nvml_collector.h"

#include <chrono>
#include <iostream>

namespace volta {
namespace agent {
namespace collectors {

NvmlCollector::NvmlCollector() = default;

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
    nvmlShutdown();
    return false;
  }

  initialized_ = true;
  return true;
}

bool NvmlCollector::IsSupported() {
  nvmlReturn_t result = nvmlInit();
  if (result != NVML_SUCCESS) {
    return false;
  }

  unsigned int device_count = 0;
  result = nvmlDeviceGetCount(&device_count);
  if (result != NVML_SUCCESS || device_count == 0) {
    nvmlShutdown();
    return false;
  }

  nvmlDevice_t device;
  result = nvmlDeviceGetHandleByIndex(0, &device);
  nvmlShutdown();
  return result == NVML_SUCCESS;
}

std::vector<Metric> NvmlCollector::Collect() {
  if (!initialized_) return {};

  std::vector<Metric> metrics;
  unsigned int power_mw = 0;
  auto now = std::chrono::system_clock::now().time_since_epoch().count();

  nvmlReturn_t result = nvmlDeviceGetPowerUsage(device_handle_, &power_mw);
  if (result == NVML_SUCCESS) {
    metrics.push_back({v1::MetricType::METRIC_TYPE_GPU_POWER,
                       {.index = 0},
                       static_cast<double>(power_mw) / 1000.0,
                       now});
  }

  unsigned int temp_c = 0;
  result =
      nvmlDeviceGetTemperature(device_handle_, NVML_TEMPERATURE_GPU, &temp_c);
  if (result == NVML_SUCCESS) {
    metrics.push_back({v1::MetricType::METRIC_TYPE_GPU_TEMPERATURE,
                       {.index = 0},
                       static_cast<double>(temp_c),
                       now});
  }

  nvmlUtilization_t utilization;
  result = nvmlDeviceGetUtilizationRates(device_handle_, &utilization);
  if (result == NVML_SUCCESS) {
    metrics.push_back({v1::MetricType::METRIC_TYPE_GPU_UTILIZATION,
                       {.index = 0},
                       static_cast<double>(utilization.gpu),
                       now});
    metrics.push_back(
        {v1::MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION,
         {.index = 0},
         static_cast<double>(utilization.memory),
         now});
  }

  nvmlMemory_t memory;
  result = nvmlDeviceGetMemoryInfo(device_handle_, &memory);
  if (result == NVML_SUCCESS) {
    metrics.push_back(
        {v1::MetricType::METRIC_TYPE_GPU_VRAM_USED,
         {.index = 0},
         static_cast<double>(memory.used) / static_cast<double>(memory.total),
         now});
  }

  return metrics;
}

std::vector<v1::MetricType> NvmlCollector::Satisfiable() {
  return {v1::MetricType::METRIC_TYPE_GPU_VRAM_USED,
          v1::MetricType::METRIC_TYPE_GPU_UTILIZATION,
          v1::MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION,
          v1::MetricType::METRIC_TYPE_GPU_TEMPERATURE,
          v1::MetricType::METRIC_TYPE_GPU_POWER};
}

}  // namespace collectors
}  // namespace agent
}  // namespace volta
