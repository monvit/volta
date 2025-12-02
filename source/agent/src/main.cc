#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

#include "scheduler.h"
#include "config/config_loader.h"
#include "config/config.h"
#include "platform/platform_detector.h"
#include "collectors/collector.h"
#include "collectors/ram_collector.h"
#include "collectors/nvml_collector.h"
#include "collectors/proc_stat_collector.h"

using namespace volta::agent;

int main() {
    try {
        auto config = config::ConfigLoader::LoadConfig();

        platform::PlatformDetector detector;
        auto hw = detector.Detect();

        std::vector<std::unique_ptr<collectors::Collector>> active_collectors;

        active_collectors.push_back(std::make_unique<collectors::ProcStatCollector>());
        
        active_collectors.push_back(std::make_unique<collectors::RamCollector>());

        for (const auto& gpu : hw.gpus) {
            if (gpu.vendor == platform::GpuVendor::NVIDIA) {
                auto nvml = std::make_unique<collectors::NvmlCollector>();
                if (nvml->Init()) {
                    active_collectors.push_back(std::move(nvml));
                }
            }
        }

        Scheduler scheduler(config, std::move(active_collectors));
        scheduler.Run();

    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
