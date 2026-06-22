#include "platform/nvml_loader.h"

#include <dlfcn.h>
#include <fmt/format.h>

#include <string>
#include <string_view>

namespace volta {
namespace agent {
namespace platform {
namespace {

NvmlApi g_nvml;
std::string g_load_error;
void* g_nvml_handle = nullptr;

template <typename T>
bool ResolveSymbol(void* handle, const char* symbol, T* out) {
  dlerror();
  auto fn = reinterpret_cast<T>(dlsym(handle, symbol));
  const char* error = dlerror();
  if (error != nullptr || fn == nullptr) {
    g_load_error = fmt::format("dlsym({}): {}", symbol,
                               error != nullptr ? error : "null symbol");
    return false;
  }
  *out = fn;
  return true;
}

constexpr std::string_view kLibraryCandidates[] = {
    "libnvidia-ml.so.1",
    "libnvidia-ml.so",
};

bool ResolveAllSymbols(void* handle) {
  return ResolveSymbol(handle, "nvmlInit_v2", &g_nvml.Init) &&
         ResolveSymbol(handle, "nvmlShutdown", &g_nvml.Shutdown) &&
         ResolveSymbol(handle, "nvmlErrorString", &g_nvml.ErrorString) &&
         ResolveSymbol(handle, "nvmlDeviceGetCount_v2",
                       &g_nvml.DeviceGetCount) &&
         ResolveSymbol(handle, "nvmlDeviceGetHandleByIndex_v2",
                       &g_nvml.DeviceGetHandleByIndex) &&
         ResolveSymbol(handle, "nvmlDeviceGetPciInfo_v3",
                       &g_nvml.DeviceGetPciInfo) &&
         ResolveSymbol(handle, "nvmlDeviceGetPowerUsage",
                       &g_nvml.DeviceGetPowerUsage) &&
         ResolveSymbol(handle, "nvmlDeviceGetTemperature",
                       &g_nvml.DeviceGetTemperature) &&
         ResolveSymbol(handle, "nvmlDeviceGetUtilizationRates",
                       &g_nvml.DeviceGetUtilizationRates) &&
         ResolveSymbol(handle, "nvmlDeviceGetMemoryInfo",
                       &g_nvml.DeviceGetMemoryInfo) &&
         ResolveSymbol(handle, "nvmlDeviceGetName", &g_nvml.DeviceGetName);
}

}  // namespace

std::string_view NvmlLoadError() { return g_load_error; }

const NvmlApi* TryLoadNvml() {
  if (g_nvml_handle != nullptr) {
    return &g_nvml;
  }

  void* handle = nullptr;
  for (const auto candidate : kLibraryCandidates) {
    dlerror();
    handle = dlopen(std::string(candidate).c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (handle != nullptr) {
      g_nvml.library_path = candidate;
      break;
    }
    if (g_load_error.empty()) {
      const char* error = dlerror();
      if (error != nullptr) {
        g_load_error = error;
      }
    }
  }

  if (handle == nullptr) {
    if (g_load_error.empty()) {
      g_load_error = "libnvidia-ml not found in search paths";
    }
    return nullptr;
  }

  if (!ResolveAllSymbols(handle)) {
    dlclose(handle);
    return nullptr;
  }

  g_nvml_handle = handle;
  g_load_error.clear();
  return &g_nvml;
}

}  // namespace platform
}  // namespace agent
}  // namespace volta
