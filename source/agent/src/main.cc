#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "client/volta_collector_client.h"
#include "collectors/collector.h"
#include "collectors/nvml_collector.h"
#include "collectors/proc_stat_collector.h"
#include "collectors/ram_collector.h"
#include "config/config.h"
#include "config/config_loader.h"
#include "platform/platform_detector.h"
#include "scheduler.h"

using namespace volta::agent;

int main() {
    auto channel = client::VoltaCollectorClient::CreateChannel("localhost:5000");
    client::VoltaCollectorClient client(channel);

    std::cout << "\n\nsend message: \n";
    client.SendMessage("Hello, World!");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "\n\nsend messages: \n";
    client.SendMessages();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "\n\nget responses: \n";
    client.GetResponses();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "\n\ntalk: \n";
    client.Talk();

    // try {
    //     auto config = config::ConfigLoader::LoadConfig();
    //
    //     platform::PlatformDetector detector;
    //     auto hw = detector.Detect();
    //     detector.PrintDetectedInfo(hw);
    //
    //     std::vector<std::unique_ptr<collectors::Collector>> active_collectors;
    //
    //     active_collectors.push_back(std::make_unique<collectors::ProcStatCollector>());
    //
    //     active_collectors.push_back(std::make_unique<collectors::RamCollector>());
    //
    //     for (const auto &gpu : hw.gpus) {
    //         if (gpu.vendor == platform::GpuVendor::NVIDIA) {
    //             auto nvml = std::make_unique<collectors::NvmlCollector>();
    //             if (nvml->Init()) {
    //                 active_collectors.push_back(std::move(nvml));
    //             }
    //         }
    //     }
    //
    //     Scheduler scheduler(config, std::move(active_collectors));
    //     scheduler.Run();
    //
    // } catch (const std::exception &e) {
    //     std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
    //     return 1;
    // }
    return 0;
}
