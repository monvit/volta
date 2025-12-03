#include "collectors/ram_collector.h"

#include <sstream>
#include <fstream>
#include <chrono>

namespace volta {
namespace agent {
namespace collectors {

std::vector<Metric> RamCollector::Collect() {
    uint64_t used = 0;
    uint64_t total = 0;

    ReadStats(used, total);

    auto now = std::chrono::system_clock::now().time_since_epoch().count();

    return {
        {"ram_total_bytes", (double) total, now},
        {"ram_used_bytes", (double) used, now},
        {"ram_used_percent", (double) used / total * 100.0, now}
    };
}

void RamCollector::ReadStats(uint64_t& used, uint64_t& total) {
    std::ifstream file("/proc/meminfo");
    std::string line, key;
    uint64_t value;
    std::string unit; // "kB"

    uint64_t available = 0;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        iss >> key >> value >> unit;
        if (key == "MemTotal:") total = value * 1024; // kB to bytes
        else if (key == "MemAvailable:") available = value * 1024;
        
        if (total > 0 && available > 0) break;
    }

    used = total - available;
}

} // namespace collectors
} // namespace agent
} // namespace volta
