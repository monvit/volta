#include "collectors/ram_collector.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace volta {
namespace agent {
namespace collectors {

bool RamCollector::Init() {
  initialized_ = true;
  return true;
}

std::vector<Metric> RamCollector::Collect() {
  if (!initialized_) return {};

  uint64_t used = 0;
  uint64_t total = 0;
  ReadStats(used, total);

  auto now = std::chrono::system_clock::now().time_since_epoch().count();
  return {{v1::MetricType::METRIC_TYPE_RAM_TOTAL,
           {.name = "ram"},
           (double)total,
           now},
          {v1::MetricType::METRIC_TYPE_RAM_USED,
           {.name = "ram"},
           (double)used,
           now}};
}

std::vector<v1::MetricType> RamCollector::Satisfiable() {
  return {v1::MetricType::METRIC_TYPE_RAM_TOTAL,
          v1::MetricType::METRIC_TYPE_RAM_USED};
}

void RamCollector::ReadStats(uint64_t& used, uint64_t& total) {
  std::ifstream file("/proc/meminfo");
  std::string line, key;
  uint64_t value;
  std::string unit;
  uint64_t available = 0;

  while (std::getline(file, line)) {
    std::istringstream iss(line);
    iss >> key >> value >> unit;
    if (key == "MemTotal:") {
      total = value * 1024;
    } else if (key == "MemAvailable:") {
      available = value * 1024;
    }
    if (total > 0 && available > 0) break;
  }

  used = total - available;
}

bool RamCollector::IsSupported() {
  return std::filesystem::exists("/proc/meminfo");
}

}  // namespace collectors
}  // namespace agent
}  // namespace volta
