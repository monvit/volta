#include "collectors/proc_stat_collector.h"

#include <sstream>
#include <chrono>

namespace volta {
namespace agent {
namespace collectors {

std::vector<Metric> ProcStatCollector::Collect() {
    uint64_t current_total = 0;
    uint64_t current_idle = 0;
    
    ReadCpuStats(current_total, current_idle);

    uint64_t diff_total = current_total - prev_total_;
    uint64_t diff_idle = current_idle - prev_idle_;
    
    double usage_percent = 0.0;
    if (diff_total > 0) usage_percent = (double)(diff_total - diff_idle) / diff_total * 100.0;

    prev_total_ = current_total;
    prev_idle_ = current_idle;

    Metric m;
    m.name = "cpu_usage_total_percent";
    m.value = usage_percent;
    m.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return {m};
}

void ProcStatCollector::ReadCpuStats(uint64_t& total, uint64_t& idle) {
    std::ifstream file("/proc/stat");
    std::string line;

    if (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string cpu_label;
        uint64_t user, nice, system, cur_idle, iowait, irq, softirq, steal;
        
        iss >> cpu_label >> user >> nice >> system >> cur_idle >> iowait >> irq >> softirq >> steal;
        
        idle = cur_idle + iowait;
        total = user + nice + system + idle + irq + softirq + steal;
    }
}


}
}
}