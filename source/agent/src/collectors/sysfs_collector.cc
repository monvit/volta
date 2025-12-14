#include "sysfs_collector.h"
#include <fstream>
#include <iostream>

namespace volta {
namespace agent {
namespace collectors {

const std::string AMD_VENDOR_ID = "0x1002";

bool SysfsCollector::Init() {
    const std::filesystem::path drm_path("/sys/class/drm");

    if (!std::filesystem::exists(drm_path) || !std::filesystem::is_directory(drm_path)) {
        return false;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(drm_path)) {
            std::string filename = entry.path().filename().string();
            if (filename.find("card") != 0 || filename.find("-") != std::string::npos) {
                continue;
            }
            
            std::filesystem::path device_path = entry.path() / "device";
            std::filesystem::path vendor_path = device_path / "vendor";
            
            std::ifstream vendor_file(vendor_path);
            if (!vendor_file.is_open()) {
                continue;
            }

            std::string vendor_id;
            vendor_file >> vendor_id;

            if (vendor_id == AMD_VENDOR_ID) {
                gpu_ = device_path;
                FindHwmonPath();
                return true;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        return false;
    }

    return false;
}

void SysfsCollector::FindHwmonPath() {
    if (!gpu_) return;

    std::filesystem::path hwmon_base = *gpu_ / "hwmon";
    if (!std::filesystem::exists(hwmon_base) || !std::filesystem::is_directory(hwmon_base)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(hwmon_base)) {
        if (std::filesystem::is_directory(entry.path())) {
            hwmon_ = entry.path();
            return;
        }
    }
}

long SysfsCollector::ReadLongFromFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (file.is_open()) {
        long value;
        file >> value;
        return value;
    }
    return -1; // TODO: handle error properly, std::optional?
}

std::vector<Metric> SysfsCollector::Collect() {
    std::vector<Metric> metrics;
    if (!gpu_) {
        return metrics;
    }

    auto now = std::chrono::system_clock::now().time_since_epoch().count();

    long load = ReadLongFromFile(*gpu_ / "gpu_busy_percent");
    if (load >= 0) {
        metrics.push_back({"gpu_usage_percent", static_cast<double>(load), now});
    }

    long vram_used = ReadLongFromFile(*gpu_ / "mem_info_vram_used");
    if (vram_used >= 0) {
        metrics.push_back({"gpu_vram_used_bytes", static_cast<double>(vram_used), now});
    }

    long vram_total = ReadLongFromFile(*gpu_ / "mem_info_vram_total");
    if (vram_total >= 0) {
        metrics.push_back({"gpu_vram_total_bytes", static_cast<double>(vram_total), now});
    }

    if (hwmon_) {
        // milli-degrees C -> Degrees C
        long temp_milli = ReadLongFromFile(*hwmon_ / "temp1_input");
        if (temp_milli >= 0) {
            metrics.push_back({"gpu_temp_celsius", temp_milli / 1000.0, now});
        }

        std::string power_path = *hwmon_ / "power1_average";
        if (!std::filesystem::exists(power_path)) {
            power_path = *hwmon_ / "power1_input";
        }
        
        long power_micro = ReadLongFromFile(power_path);
        if (power_micro >= 0) {
            metrics.push_back({"gpu_power_watts", power_micro / 1000000.0, now});
        }
    }

    return metrics;
}

} // namespace collectors
} // namespace agent
} // namespace volta
