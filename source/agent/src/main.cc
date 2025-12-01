#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

#include "config/config_loader.h"
#include "config/config.h"
#include "platform/platform_detector.h"
#include "collectors/collector.h"
#include "collectors/nvml_collector.h"
#include "collectors/proc_stat_collector.h"

using namespace volta::agent;
using namespace volta::agent::config;
using namespace volta::agent::collectors;
using namespace volta::agent::platform;

int main(int argc, char** argv) {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "   VOLTA AGENT - LOCAL METRIC TESTER    " << std::endl;
        std::cout << "========================================" << std::endl;

        std::cout << "[Init] Loading configuration..." << std::endl;
        Config config = ConfigLoader::LoadConfig();

        std::cout << "[Init] Detecting hardware..." << std::endl;
        PlatformDetector detector;
        HardwareInfo hw_info = detector.Detect();

        std::cout << "       Host: " << hw_info.hostname << std::endl;
        std::cout << "       GPUs detected: " << hw_info.gpus.size() << std::endl;

        std::vector<std::unique_ptr<Collector>> active_collectors;

        if (config.collectors.count(CollectorNames::kNvml) && 
            config.collectors.at(CollectorNames::kNvml).enabled) {
            
            bool has_nvidia = false;
            for (const auto& gpu : hw_info.gpus) {
                if (gpu.vendor == GpuVendor::NVIDIA) {
                    has_nvidia = true;
                    break;
                }
            }

            if (has_nvidia) {
                std::cout << "[Init] + Adding NVML Collector" << std::endl;
                auto nvml = std::make_unique<NvmlCollector>();
                if (nvml->Init()) {
                    active_collectors.push_back(std::move(nvml));
                } else {
                    std::cerr << "[Error] Failed to init NVML." << std::endl;
                }
            }
        }

        if (config.collectors.count(CollectorNames::kProcStat) && 
            config.collectors.at(CollectorNames::kProcStat).enabled) {
            std::cout << "[Init] + Adding ProcStat Collector" << std::endl;
            active_collectors.push_back(std::make_unique<ProcStatCollector>());
        }

        if (active_collectors.empty()) {
            std::cerr << "[Error] No collectors created! Check Config or Hardware." << std::endl;
            return 1;
        }

        std::cout << "\n[Test] Starting measurement loop (5 iterations)..." << std::endl;

        for (int i = 1; i <= 5; ++i) {
            std::cout << "\n--- Cycle " << i << "/5 ---" << std::endl;

            for (const auto& collector : active_collectors) {
                std::vector<Metric> metrics = collector->Collect();

                for (const auto& m : metrics) {
                    std::cout << "   [METRIC] " << m.name << ": " << m.value << std::endl;
                }
            }

            if (i < 5) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        std::cout << "\n[Test] Done. Exiting." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "CRITICAL EXCEPTION: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}