#include "scheduler.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace volta {
namespace agent {

Scheduler::Scheduler(const config::Config& config,
                     std::vector<collectors::Collector*>&& collectors)
    : collectors_(std::move(collectors)), config_(config), buffer_(config) {}

void Scheduler::Run() {
  for (auto collector : collectors_) {
    collector->Init();
  }
  std::cout << "[" << config_.uuid << "] Starting collection loop (Interval: "
            << config_.collection_interval.count() << "ms)..." << std::endl;

  while (true) {
    for (const auto& collector : collectors_) {
      auto metrics = collector->Collect();
      for (const auto& metric : metrics) {
        buffer_.AddMetric(metric);
      }
    }

    PrintDashboard();

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
  std::cout << "    VOLTA AGENT v0.5 - ACTIVE MONITOR    \n";
  std::cout << "===============================================\n";

  std::cout << std::left << std::setw(42) << "METRIC NAME" << std::setw(38)
            << "DEVICE"
            << "    " << std::fixed << "VALUE"
            << "\n";
  std::cout << "-----------------------------------------------\n";

  auto latest = buffer_.LatestSamples();
  for (const auto& [key, sample] : latest) {
    auto metric_name =
        MetricType_Name(static_cast<MetricType>(key.metric_type));
    const std::string prefix = "METRIC_TYPE_";
    if (metric_name.rfind(prefix, 0) == 0) {
      metric_name = metric_name.substr(prefix.size());
    }

    std::cout << std::left << std::setw(42) << metric_name << std::setw(38)
              << DescribeKey(key) << "    " << std::fixed
              << std::setprecision(2) << sample.value << " @ "
              << sample.timestamp_ns << "\n";
  }

  std::cout << "-----------------------------------------------\n";
  std::cout << "Data points collected: " << latest.size() << "\n";
  std::cout << "# of metrics buffered: " << buffer_.CapacityPerSeries() << "\n";
  std::cout << "Press Ctrl+C to exit."
            << "\n";
  std::cout.flush();
}

}  // namespace agent
}  // namespace volta
