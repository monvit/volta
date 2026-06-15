#include "scheduler.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace {

std::string FormatFixed(double value, const char* unit, int precision = 2) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(precision) << value;
  if (unit != nullptr && *unit != '\0') {
    out << ' ' << unit;
  }
  return out.str();
}

std::string FormatBytesMiB(double bytes) {
  return FormatFixed(bytes / (1024.0 * 1024.0), "MiB");
}

std::string FormatPercent(double value) { return FormatFixed(value, "%"); }

std::string FormatTemperatureC(double value) {
  return FormatFixed(value, "°C");
}

std::string FormatPowerW(double watts) { return FormatFixed(watts, "W"); }

std::string FormatEnergyJ(double joules) { return FormatFixed(joules, "J"); }

std::string FormatMetricValue(volta::MetricType type, double value,
                              std::chrono::milliseconds interval) {
  using volta::MetricType;

  switch (type) {
    case MetricType::METRIC_TYPE_CPU_UTILIZATION:
    case MetricType::METRIC_TYPE_GPU_UTILIZATION:
    case MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION:
    case MetricType::METRIC_TYPE_CPU_IOWAIT:
    case MetricType::METRIC_TYPE_CPU_CACHE_HIT_RATIO:
    case MetricType::METRIC_TYPE_SWAP_ACTIVITY:
    case MetricType::METRIC_TYPE_DISK_BUSY_TIME:
      return FormatPercent(value);

    case MetricType::METRIC_TYPE_GPU_VRAM_USED:
      // NVML reports this collector as a ratio of used / total.
      return FormatPercent(value * 100.0);

    case MetricType::METRIC_TYPE_GPU_TEMPERATURE:
    case MetricType::METRIC_TYPE_CPU_TEMPERATURE:
      return FormatTemperatureC(value);

    case MetricType::METRIC_TYPE_GPU_POWER:
    case MetricType::METRIC_TYPE_CPU_POWER_CORES:
    case MetricType::METRIC_TYPE_RAM_POWER:
      return FormatPowerW(value);

    case MetricType::METRIC_TYPE_CPU_POWER_PACKAGE: {
      const double seconds = std::chrono::duration<double>(interval).count();
      if (seconds > 0.0) {
        return FormatPowerW(value / seconds);
      }
      return FormatEnergyJ(value);
    }

    case MetricType::METRIC_TYPE_RAM_TOTAL:
    case MetricType::METRIC_TYPE_RAM_AVAILABLE:
    case MetricType::METRIC_TYPE_RAM_USED:
    case MetricType::METRIC_TYPE_RAM_CACHED:
    case MetricType::METRIC_TYPE_SWAP_USED:
    case MetricType::METRIC_TYPE_DISK_CAPACITY_USED:
    case MetricType::METRIC_TYPE_NET_BYTES_RECEIVED:
    case MetricType::METRIC_TYPE_NET_BYTES_SENT:
      return FormatBytesMiB(value);

    case MetricType::METRIC_TYPE_DISK_READ_THROUGHPUT:
    case MetricType::METRIC_TYPE_DISK_WRITE_THROUGHPUT:
      return FormatFixed(value / (1024.0 * 1024.0), "MiB/s");

    default:
      return FormatFixed(value, "");
  }
}

}  // namespace

namespace volta {
namespace agent {

Scheduler::Scheduler(const config::Config& config,
                     std::vector<collectors::Collector*>&& collectors,
                     std::shared_ptr<MetricsBuffer> buffer)
    : collectors_(std::move(collectors)), config_(config), buffer_(buffer) {}

void Scheduler::Run() {
  for (auto collector : collectors_) {
    collector->Init();
  }

  std::cout << "Starting collection loop (Interval: "
            << config_.collection_interval.count() << "ms)..." << std::endl;

  while (true) {
    for (const auto& collector : collectors_) {
      // TODO: make the metrics collect into a preallocated tray
      //       instead of allocating new memory for each collection
      auto metrics = collector->Collect();
      buffer_->AddMetrics(metrics);
    }

    if (print_dashboard.load()) PrintDashboard();

    std::this_thread::sleep_for(config_.collection_interval);
  }
}

std::string Scheduler::DescribeKey(const BufferKey& key) {
  std::ostringstream out;
  if (key.pci_domain || key.pci_bus || key.pci_device || key.pci_function) {
    out << "gpu=" << key.pci_domain << ':' << static_cast<int>(key.pci_bus)
        << ':' << static_cast<int>(key.pci_device) << '.'
        << static_cast<int>(key.pci_function);
  } else if (key.socket_index || key.core_index) {
    out << "cpu=" << static_cast<int>(key.socket_index) << "/"
        << key.core_index;
  } else if (key.ifindex) {
    out << "net_ifindex=" << key.ifindex;
  } else if (key.disk_major || key.disk_minor) {
    out << "disk=" << key.disk_major << ':' << key.disk_minor;
  } else {
    out << "system";
  }
  return out.str();
}

void Scheduler::PrintDashboard() {
  // ansi clean screen, move cursor to top-left
  std::cout << "\033[2J\033[1;1H";

  std::cout << "===============================================\n";
  std::cout << "    VOLTA AGENT - ACTIVE MONITOR    \n";
  std::cout << "===============================================\n";

  std::cout << std::left << std::setw(35) << "METRIC NAME" << std::setw(20)
            << "DEVICE" << std::setw(18) << "VALUE" << "\n";
  std::cout << "-----------------------------------------------\n";

  auto latest = buffer_->LatestSamples();
  for (const auto& [key, sample] : latest) {
    auto metric_name =
        MetricType_Name(static_cast<MetricType>(key.metric_type));
    const std::string prefix = "METRIC_TYPE_";
    if (metric_name.rfind(prefix, 0) == 0) {
      metric_name = metric_name.substr(prefix.size());
    }

    std::cout << std::left << std::setw(35) << metric_name << std::setw(20)
              << DescribeKey(key) << std::setw(10)
              << FormatMetricValue(static_cast<MetricType>(key.metric_type),
                                   sample.value, config_.collection_interval)
              << '\n';
  }

  std::cout << "-----------------------------------------------\n";
  std::cout << "Data points collected: " << latest.size() << "\n";
  std::cout << "# of metrics buffered: " << buffer_->CapacityPerSeries()
            << "\n";
  std::cout << "Press Ctrl+C to exit."
            << "\n";
  std::cout.flush();
}

}  // namespace agent
}  // namespace volta
