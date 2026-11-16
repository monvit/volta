#include "collectors/nvml_collector.h"

#include <algorithm>
#include <chrono>

namespace volta {
namespace agent {
namespace collectors {

NvmlCollector::NvmlCollector() = default;

NvmlCollector::~NvmlCollector() {
  if (initialized_) {
    nvml_->Shutdown();
    initialized_ = false;
    nvml_ = nullptr;
  }
  platform::UnloadNvml();
}

bool NvmlCollector::Start(const std::vector<MetricType>& metrics) {
  requested_metrics_ = metrics;
  if (requested_metrics_.empty()) return false;

  const platform::NvmlApi* nvml = platform::TryLoadNvml();
  if (nvml == nullptr) {
    logger_->Log(logging::LogLevel::WARNING_LEVEL, "Failed to load NVML: {}",
                 platform::NvmlLoadError());
    return false;
  }

  nvmlReturn_t result = nvml->Init();
  if (result != NVML_SUCCESS) {
    logger_->Log(logging::LogLevel::WARNING_LEVEL,
                 "Failed to initialize NVML: {}", nvml->ErrorString(result));
    platform::UnloadNvml();
    return false;
  }

  auto fail_after_init = [nvml]() {
    nvml->Shutdown();
    platform::UnloadNvml();
    return false;
  };

  unsigned int device_count = 0;
  result = nvml->DeviceGetCount(&device_count);
  if (result != NVML_SUCCESS) {
    logger_->Log(logging::LogLevel::WARNING_LEVEL,
                 "Failed to get NVML device count: {}",
                 nvml->ErrorString(result));
    return fail_after_init();
  }
  if (device_count == 0) {
    logger_->Log(logging::LogLevel::DEBUG_LEVEL,
                 "No NVIDIA devices found; disabling NVML collector");
    return fail_after_init();
  }

  result = nvml->DeviceGetHandleByIndex(0, &device_handle_);
  if (result != NVML_SUCCESS) {
    logger_->Log(logging::LogLevel::WARNING_LEVEL,
                 "Failed to get device handle: {}", nvml->ErrorString(result));
    return fail_after_init();
  }

  nvmlPciInfo_t pci_info;
  result = nvml->DeviceGetPciInfo(device_handle_, &pci_info);
  if (result != NVML_SUCCESS) {
    logger_->Log(logging::LogLevel::WARNING_LEVEL, "Failed to get PCI info: {}",
                 nvml->ErrorString(result));
    return fail_after_init();
  }

  GpuID gpu_id;
  gpu_id.set_pci_domain(pci_info.domain);
  gpu_id.set_pci_bus(pci_info.bus);
  gpu_id.set_pci_device(pci_info.device);
  gpu_id.set_pci_function(0);
  gpu_id_ = std::move(gpu_id);

  nvml_ = nvml;
  initialized_ = true;
  return true;
}

void NvmlCollector::Collect(std::vector<Metric>& metrics) {
  if (!initialized_ || requested_metrics_.empty()) return;

  const auto& nvml = *nvml_;
  unsigned int power_mw = 0;
  auto now = std::chrono::system_clock::now().time_since_epoch().count();

  auto needs = [&](MetricType type) {
    return std::find(requested_metrics_.begin(), requested_metrics_.end(),
                     type) != requested_metrics_.end();
  };

  nvmlReturn_t result;
  if (needs(MetricType::METRIC_TYPE_GPU_POWER)) {
    result = nvml.DeviceGetPowerUsage(device_handle_, &power_mw);
    if (result == NVML_SUCCESS) {
      metrics.push_back({MetricType::METRIC_TYPE_GPU_POWER, gpu_id_,
                         static_cast<double>(power_mw) / 1000.0, now});
    }
  }

  if (needs(MetricType::METRIC_TYPE_GPU_TEMPERATURE)) {
    unsigned int temp_c = 0;
    result = nvml.DeviceGetTemperature(device_handle_, NVML_TEMPERATURE_GPU,
                                       &temp_c);
    if (result == NVML_SUCCESS) {
      metrics.push_back({MetricType::METRIC_TYPE_GPU_TEMPERATURE, gpu_id_,
                         static_cast<double>(temp_c), now});
    }
  }

  if (needs(MetricType::METRIC_TYPE_GPU_UTILIZATION) ||
      needs(MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION)) {
    nvmlUtilization_t utilization;
    result = nvml.DeviceGetUtilizationRates(device_handle_, &utilization);
    if (result == NVML_SUCCESS) {
      if (needs(MetricType::METRIC_TYPE_GPU_UTILIZATION)) {
        metrics.push_back({MetricType::METRIC_TYPE_GPU_UTILIZATION, gpu_id_,
                           static_cast<double>(utilization.gpu), now});
      }
      if (needs(MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION)) {
        metrics.push_back(
            {MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION, gpu_id_,
             static_cast<double>(utilization.memory), now});
      }
    }
  }

  if (needs(MetricType::METRIC_TYPE_GPU_VRAM_USED)) {
    nvmlMemory_t memory;
    result = nvml.DeviceGetMemoryInfo(device_handle_, &memory);
    if (result == NVML_SUCCESS) {
      metrics.push_back(
          {MetricType::METRIC_TYPE_GPU_VRAM_USED, gpu_id_,
           static_cast<double>(memory.used) / static_cast<double>(memory.total),
           now});
    }
  }
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
