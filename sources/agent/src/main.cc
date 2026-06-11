#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "collectors/collector.h"
#include "collectors/nvml_collector.h"
#include "collectors/proc_stat_collector.h"
#include "collectors/ram_collector.h"
#include "collectors/rapl_collector.h"
#include "config/config.h"
#include "config/config_loader.h"
#include "platform/platform_detector.h"
#include "scheduler.h"

using namespace volta::agent;

int main() {
  try {
    auto config = config::ConfigLoader::LoadConfig();

    // platform::PlatformDetector detector;
    // auto hw = detector.Detect();
    // detector.PrintDetectedInfo(hw);

    auto active_collectors = collectors::CollectorRegistry::Instance().Resolve(
        config.requestedMetrics);

    std::cin.get();

    Scheduler scheduler(config, std::move(active_collectors));
    scheduler.Run();

  } catch (const std::exception& e) {
    std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
