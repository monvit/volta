#ifndef VOLTA_AGENT_SRC_PLATFORM_NVML_LOADER_H_
#define VOLTA_AGENT_SRC_PLATFORM_NVML_LOADER_H_

#ifndef NVML_NO_UNVERSIONED_FUNC_DEFS
#define NVML_NO_UNVERSIONED_FUNC_DEFS
#endif
#include <nvml.h>

#include <string>
#include <string_view>

namespace volta {
namespace agent {
namespace platform {

struct NvmlApi {
  std::string library_path;

  nvmlReturn_t (*Init)(void) = nullptr;
  nvmlReturn_t (*Shutdown)(void) = nullptr;
  const char* (*ErrorString)(nvmlReturn_t) = nullptr;
  nvmlReturn_t (*DeviceGetCount)(unsigned int*) = nullptr;
  nvmlReturn_t (*DeviceGetHandleByIndex)(unsigned int, nvmlDevice_t*) = nullptr;
  nvmlReturn_t (*DeviceGetPciInfo)(nvmlDevice_t, nvmlPciInfo_t*) = nullptr;
  nvmlReturn_t (*DeviceGetPowerUsage)(nvmlDevice_t, unsigned int*) = nullptr;
  nvmlReturn_t (*DeviceGetTemperature)(nvmlDevice_t, nvmlTemperatureSensors_t,
                                       unsigned int*) = nullptr;
  nvmlReturn_t (*DeviceGetUtilizationRates)(nvmlDevice_t,
                                            nvmlUtilization_t*) = nullptr;
  nvmlReturn_t (*DeviceGetMemoryInfo)(nvmlDevice_t, nvmlMemory_t*) = nullptr;
  nvmlReturn_t (*DeviceGetName)(nvmlDevice_t, char*, unsigned int) = nullptr;
};

const NvmlApi* TryLoadNvml();

std::string_view NvmlLoadError();

}  // namespace platform
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_PLATFORM_NVML_LOADER_H_
