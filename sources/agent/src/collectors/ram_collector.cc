#include "collectors/ram_collector.h"

#include <algorithm>
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

void RamCollector::SetRequestedMetrics(const std::vector<MetricType>& metrics) {
  requested_metrics_ = metrics;
}

std::vector<Metric> RamCollector::Collect() {
  if (!initialized_ || requested_metrics_.empty()) return {};

  bool needs_total =
      std::find(requested_metrics_.begin(), requested_metrics_.end(),
                MetricType::METRIC_TYPE_RAM_TOTAL) != requested_metrics_.end();
  bool needs_used =
      std::find(requested_metrics_.begin(), requested_metrics_.end(),
                MetricType::METRIC_TYPE_RAM_USED) != requested_metrics_.end();
  if (!needs_total && !needs_used) return {};

  uint64_t used = 0;
  uint64_t total = 0;
  ReadStats(used, total);

  auto now = std::chrono::system_clock::now().time_since_epoch().count();
  std::vector<Metric> metrics;
  if (needs_total) {
    metrics.push_back(
        {MetricType::METRIC_TYPE_RAM_TOTAL, std::nullopt, (double)total, now});
  }
  if (needs_used) {
    metrics.push_back(
        {MetricType::METRIC_TYPE_RAM_USED, std::nullopt, (double)used, now});
  }
  return metrics;
}

std::vector<MetricType> RamCollector::Satisfiable() {
  return {MetricType::METRIC_TYPE_RAM_TOTAL, MetricType::METRIC_TYPE_RAM_USED};
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
