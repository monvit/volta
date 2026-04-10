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

RaplCollector::RaplCollector() {
  OpenMSR();
  uint64_t readout = ReadMSR(0, MSR_RAPL::POWER_UNIT);
  power_units_ = pow(0.5, (double)(readout & 0xf));
  energy_units_ = pow(0.5, (double)((readout >> 8) & 0x1f));
  time_units_ = pow(0.5, (double)((readout >> 16) & 0xf));
  readout = ReadMSR(0, MSR_RAPL::PKG::ENERGY_STATUS);
  last_value = energy_units_ * readout;
}

std::vector<Metric> RaplCollector::Collect() {
  uint64_t readout;

  try {
    readout = ReadMSR(0, MSR_RAPL::PKG::ENERGY_STATUS);
  } catch (const MSR_Read_Exception &e) {
    return {};
  }

  double value = energy_units_ * readout;

  Metric m;
  m.name = "cpu_energy_usage_total";
  m.value = value - last_value;
  m.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  last_value = value;
  return {m};
}

uint64_t RaplCollector::ReadMSR(uint8_t core, uint32_t offset) {
  uint64_t data;
  if (core + 1 > MSR_files_.size()) {
    throw MSR_Read_Exception();
  }
  // c-like read for thread safety
  if (pread(MSR_files_[core], &data, sizeof data, offset) != sizeof data) {
    return {};
  }

  return data;
}

void RaplCollector::OpenMSR() {
  const std::filesystem::path cpu_base = "/dev/cpu";
  MSR_files_ = std::vector<int>();
  std::error_code ec;

  if (!std::filesystem::exists(cpu_base, ec)) {
    throw MSR_Open_Exception();
  }
  std::vector<std::pair<int, std::filesystem::path>> cpu_entries;
  for (const auto &entry : std::filesystem::directory_iterator(cpu_base)) {
    if (!entry.is_directory()) continue;
    const auto &dirname = entry.path().filename().string();
    if (!std::ranges::all_of(dirname, ::isdigit)) continue;
    cpu_entries.emplace_back(std::stoi(dirname), entry.path());
  }

  std::ranges::sort(cpu_entries);

  for (const auto &[id, path] : cpu_entries) {
    int fd = open((path / "msr").c_str(), O_RDONLY);
    if (fd >= 0) {
      MSR_files_.push_back(fd);
    }
  }
}

void RaplCollector::CloseMSR(int fd) { close(fd); }

RaplCollector::~RaplCollector() {
  for (auto file : MSR_files_) {
    CloseMSR(file);
  }
};
}  // namespace collectors
}  // namespace agent
}  // namespace volta
