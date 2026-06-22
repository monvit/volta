#include "collectors/nvml_collector.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <utility>

namespace volta {
namespace agent {
namespace collectors {

NvmlCollector::NvmlCollector() = default;

NvmlCollector::~NvmlCollector() {
  if (initialized_) {
    nvml_->Shutdown();
  }
}

bool NvmlCollector::Init() {
  const platform::NvmlApi* nvml = platform::TryLoadNvml();
  if (nvml == nullptr) {
    std::cerr << "Failed to load NVML: " << platform::NvmlLoadError()
              << std::endl;
    return false;
  }

  nvmlReturn_t result = nvml->Init();
  if (result != NVML_SUCCESS) {
    std::cerr << "Failed to initialize NVML: " << nvml->ErrorString(result)
              << std::endl;
    return false;
  }

  unsigned int device_count = 0;
  result = nvml->DeviceGetCount(&device_count);
  if (result != NVML_SUCCESS || device_count == 0) {
    std::cerr << "Failed to get NVML device count: "
              << nvml->ErrorString(result) << std::endl;
    nvml->Shutdown();
    return false;
  }

  devices_.clear();
  for (unsigned int i = 0; i < device_count; ++i) {
    nvmlDevice_t device_handle;
    result = nvml->DeviceGetHandleByIndex(i, &device_handle);
    if (result != NVML_SUCCESS) {
      std::cerr << "Failed to get device handle for GPU " << i << ": "
                << nvml->ErrorString(result) << std::endl;
      continue;
    }

    nvmlPciInfo_t pci_info;
    result = nvml->DeviceGetPciInfo(device_handle, &pci_info);
    if (result != NVML_SUCCESS) {
      std::cerr << "Failed to get PCI info for GPU " << i << ": "
                << nvml->ErrorString(result) << std::endl;
      continue;
    }

    GpuID gpu_id;
    gpu_id.set_pci_domain(pci_info.domain);
    gpu_id.set_pci_bus(pci_info.bus);
    gpu_id.set_pci_device(pci_info.device);
    gpu_id.set_pci_function(0);
    devices_.push_back({device_handle, std::move(gpu_id)});
  }

  if (devices_.empty()) {
    std::cerr << "Failed to discover any NVML GPUs" << std::endl;
    nvml->Shutdown();
    return false;
  }

  nvml_ = nvml;
  initialized_ = true;
  return true;
}

bool NvmlCollector::IsSupported() {
  const platform::NvmlApi* nvml = platform::TryLoadNvml();
  if (nvml == nullptr) {
    return false;
  }

  nvmlReturn_t result = nvml->Init();
  if (result != NVML_SUCCESS) {
    return false;
  }

  unsigned int device_count = 0;
  result = nvml->DeviceGetCount(&device_count);
  if (result != NVML_SUCCESS || device_count == 0) {
    nvml->Shutdown();
    return false;
  }

  nvmlDevice_t device;
  result = nvml->DeviceGetHandleByIndex(0, &device);
  nvml->Shutdown();
  return result == NVML_SUCCESS;
}

void NvmlCollector::SetRequestedMetrics(
    const std::vector<MetricType>& metrics) {
  requested_metrics_ = metrics;
}

std::vector<Metric> NvmlCollector::Collect() {
  if (!initialized_ || requested_metrics_.empty()) return {};

  const auto& nvml = *nvml_;
  std::vector<Metric> metrics;
  unsigned int power_mw = 0;
  auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();

  auto needs = [&](MetricType type) {
    return std::find(requested_metrics_.begin(), requested_metrics_.end(),
                     type) != requested_metrics_.end();
  };

  nvmlReturn_t result;
  for (const auto& device : devices_) {
    if (needs(MetricType::METRIC_TYPE_GPU_POWER)) {
      result = nvml.DeviceGetPowerUsage(device.handle, &power_mw);
      if (result == NVML_SUCCESS) {
        metrics.push_back({MetricType::METRIC_TYPE_GPU_POWER, device.id,
                           static_cast<double>(power_mw) / 1000.0, now});
      }
    }

    if (needs(MetricType::METRIC_TYPE_GPU_TEMPERATURE)) {
      unsigned int temp_c = 0;
      result = nvml.DeviceGetTemperature(device.handle, NVML_TEMPERATURE_GPU,
                                         &temp_c);
      if (result == NVML_SUCCESS) {
        metrics.push_back({MetricType::METRIC_TYPE_GPU_TEMPERATURE, device.id,
                           static_cast<double>(temp_c), now});
      }
    }

    if (needs(MetricType::METRIC_TYPE_GPU_UTILIZATION) ||
        needs(MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION)) {
      nvmlUtilization_t utilization;
      result = nvml.DeviceGetUtilizationRates(device.handle, &utilization);
      if (result == NVML_SUCCESS) {
        if (needs(MetricType::METRIC_TYPE_GPU_UTILIZATION)) {
          metrics.push_back({MetricType::METRIC_TYPE_GPU_UTILIZATION, device.id,
                             static_cast<double>(utilization.gpu), now});
        }
        if (needs(MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION)) {
          metrics.push_back(
              {MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION, device.id,
               static_cast<double>(utilization.memory), now});
        }
      }
    }

    if (needs(MetricType::METRIC_TYPE_GPU_VRAM_USED)) {
      nvmlMemory_t memory;
      result = nvml.DeviceGetMemoryInfo(device.handle, &memory);
      if (result == NVML_SUCCESS) {
        metrics.push_back({MetricType::METRIC_TYPE_GPU_VRAM_USED, device.id,
                           static_cast<double>(memory.used) /
                               static_cast<double>(memory.total),
                           now});
      }
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
