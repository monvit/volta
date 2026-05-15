#include "collectors/nvml_collector.h"

#include <algorithm>
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

void NvmlCollector::SetRequestedMetrics(
    const std::vector<MetricType>& metrics) {
  requested_metrics_ = metrics;
}

std::vector<Metric> NvmlCollector::Collect() {
  if (!initialized_ || requested_metrics_.empty()) return {};

  std::vector<Metric> metrics;
  unsigned int power_mw = 0;
  auto now = std::chrono::system_clock::now().time_since_epoch().count();

  auto needs = [&](MetricType type) {
    return std::find(requested_metrics_.begin(), requested_metrics_.end(),
                     type) != requested_metrics_.end();
  };

  nvmlReturn_t result;
  if (needs(MetricType::METRIC_TYPE_GPU_POWER)) {
    result = nvmlDeviceGetPowerUsage(device_handle_, &power_mw);
    if (result == NVML_SUCCESS) {
      metrics.push_back({MetricType::METRIC_TYPE_GPU_POWER,
                         {.index = 0},
                         static_cast<double>(power_mw) / 1000.0,
                         now});
    }
  }

  if (needs(MetricType::METRIC_TYPE_GPU_TEMPERATURE)) {
    unsigned int temp_c = 0;
    result =
        nvmlDeviceGetTemperature(device_handle_, NVML_TEMPERATURE_GPU, &temp_c);
    if (result == NVML_SUCCESS) {
      metrics.push_back({MetricType::METRIC_TYPE_GPU_TEMPERATURE,
                         {.index = 0},
                         static_cast<double>(temp_c),
                         now});
    }
  }

  if (needs(MetricType::METRIC_TYPE_GPU_UTILIZATION) ||
      needs(MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION)) {
    nvmlUtilization_t utilization;
    result = nvmlDeviceGetUtilizationRates(device_handle_, &utilization);
    if (result == NVML_SUCCESS) {
      if (needs(MetricType::METRIC_TYPE_GPU_UTILIZATION)) {
        metrics.push_back({MetricType::METRIC_TYPE_GPU_UTILIZATION,
                           {.index = 0},
                           static_cast<double>(utilization.gpu),
                           now});
      }
      if (needs(MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION)) {
        metrics.push_back(
            {MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION,
             {.index = 0},
             static_cast<double>(utilization.memory),
             now});
      }
    }
  }

  if (needs(MetricType::METRIC_TYPE_GPU_VRAM_USED)) {
    nvmlMemory_t memory;
    result = nvmlDeviceGetMemoryInfo(device_handle_, &memory);
    if (result == NVML_SUCCESS) {
      metrics.push_back(
          {MetricType::METRIC_TYPE_GPU_VRAM_USED,
           {.index = 0},
           static_cast<double>(memory.used) / static_cast<double>(memory.total),
           now});
    }
  }

  return metrics;
}

std::vector<MetricType> NvmlCollector::Satisfiable() {
  return {MetricType::METRIC_TYPE_GPU_VRAM_USED,
          MetricType::METRIC_TYPE_GPU_UTILIZATION,
          MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION,
          MetricType::METRIC_TYPE_GPU_TEMPERATURE,
          MetricType::METRIC_TYPE_GPU_POWER};
}

}  // namespace collectors
}  // namespace agent
}  // namespace volta
