#include "scheduler.h"

#include <iostream>
#include <thread>
#include <iomanip>

namespace volta {
namespace agent {

Scheduler::Scheduler(
        const config::Config& config,
        std::vector<std::unique_ptr<collectors::Collector>>&& collectors) 
    : config_(config)
    , collectors_(std::move(collectors)) {}

void Scheduler::Run() {
    std::cout << "[" << typeid(*this).name() << "] Starting collection loop (Interval: " 
              << config_.collection_interval.count() << "ms)..." << std::endl;

    while (true) {
        std::vector<Metric> batch;
        for (const auto& collector : collectors_) {
            auto metrics = collector->Collect();
            batch.insert(batch.end(), metrics.begin(), metrics.end());
        }

        PrintDashboard(batch);

        std::this_thread::sleep_for(config_.collection_interval);
    }
}

void Scheduler::PrintDashboard(const std::vector<Metric>& metrics) {
    std::cout << "\033[2J\033[1;1H"; // ansi clean screen, move cursor to top-left
    
    std::cout << "===============================================\n";
    std::cout << "    VOLTA AGENT v0.1 (POC) - ACTIVE MONITOR    \n";
    std::cout << "===============================================\n";
    
    std::cout << std::left << std::setw(30) << "METRIC NAME" << "VALUE\n";
    std::cout << "-----------------------------------------------\n";

    for (const auto& m : metrics) {
        std::cout << std::left << std::setw(30) << m.name 
                << std::fixed << std::setprecision(2) << m.value << "\n";
    }

    std::cout << "-----------------------------------------------\n";
    std::cout << "Data points collected: " << metrics.size() << "\n";
    std::cout << "Press Ctrl+C to exit." << "\n";
    std::cout << std::flush;
}

} // namespace agent
} // namespace volta
