#include "rapl_collector.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>

namespace volta {
namespace agent {
namespace collectors {

RaplCollector::RaplCollector() = default;

bool RaplCollector::Init() {
  try {
    OpenMSR();
    uint64_t readout = ReadMSR(0, MSR_RAPL::POWER_UNIT);
    power_units_ = pow(0.5, (double)(readout & 0xf));
    energy_units_ = pow(0.5, (double)((readout >> 8) & 0x1f));
    time_units_ = pow(0.5, (double)((readout >> 16) & 0xf));
    readout = ReadMSR(0, MSR_RAPL::PKG::ENERGY_STATUS);
    last_value = energy_units_ * readout;
    initialized_ = true;
    return true;
  } catch (const MSR_Open_Exception&) {
    return false;
  } catch (const MSR_Read_Exception&) {
    return false;
  }
}

bool RaplCollector::IsSupported() {
  const std::filesystem::path cpu_base = "/dev/cpu";
  std::error_code ec;
  if (!std::filesystem::exists(cpu_base, ec) ||
      !std::filesystem::is_directory(cpu_base, ec)) {
    return false;
  }

  for (const auto& entry : std::filesystem::directory_iterator(cpu_base)) {
    if (!entry.is_directory()) continue;

    const auto msr_path = entry.path() / "msr";
    if (std::filesystem::exists(msr_path, ec) &&
        access(msr_path.c_str(), R_OK) == 0) {
      return true;
    }
  }

  return false;
}

void RaplCollector::SetRequestedMetrics(
    const std::vector<v1::MetricType>& metrics) {
  requested_metrics_ = metrics;
}

std::vector<Metric> RaplCollector::Collect() {
  if (!initialized_ || requested_metrics_.empty()) return {};
  if (std::find(requested_metrics_.begin(), requested_metrics_.end(),
                v1::MetricType::METRIC_TYPE_CPU_POWER_PACKAGE) ==
      requested_metrics_.end()) {
    return {};
  }

  uint64_t readout;
  try {
    readout = ReadMSR(0, MSR_RAPL::PKG::ENERGY_STATUS);
  } catch (const MSR_Read_Exception&) {
    return {};
  }

  double value = energy_units_ * readout;
  Metric m;
  m.type = v1::MetricType::METRIC_TYPE_CPU_POWER_PACKAGE;
  m.devId = {};
  m.value = value - last_value;
  m.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
  last_value = value;
  return {m};
}

std::vector<v1::MetricType> RaplCollector::Satisfiable() {
  return {v1::MetricType::METRIC_TYPE_CPU_POWER_PACKAGE};
}

uint64_t RaplCollector::ReadMSR(uint8_t core, uint32_t offset) {
  uint64_t data;
  if (core + 1 > MSR_files_.size()) {
    throw MSR_Read_Exception();
  }
  if (pread(MSR_files_[core], &data, sizeof data, offset) != sizeof data) {
    throw MSR_Read_Exception();
  }
  return data;
}

void RaplCollector::OpenMSR() {
  const std::filesystem::path cpu_base = "/dev/cpu";
  MSR_files_.clear();
  std::error_code ec;

  if (!std::filesystem::exists(cpu_base, ec)) {
    throw MSR_Open_Exception();
  }
  std::vector<std::pair<int, std::filesystem::path>> cpu_entries;
  for (const auto& entry : std::filesystem::directory_iterator(cpu_base)) {
    if (!entry.is_directory()) continue;
    const auto& dirname = entry.path().filename().string();
    if (!std::ranges::all_of(dirname, ::isdigit)) continue;
    cpu_entries.emplace_back(std::stoi(dirname), entry.path());
  }

  std::ranges::sort(cpu_entries);
  for (const auto& [id, path] : cpu_entries) {
    int fd = open((path / "msr").c_str(), O_RDONLY);
    if (fd >= 0) {
      MSR_files_.push_back(fd);
    }
  }
  if (MSR_files_.empty()) {
    throw MSR_Open_Exception();
  }
}

void RaplCollector::CloseMSR(int fd) { close(fd); }

RaplCollector::~RaplCollector() {
  for (auto file : MSR_files_) {
    CloseMSR(file);
  }
}

}  // namespace collectors
}  // namespace agent
}  // namespace volta
