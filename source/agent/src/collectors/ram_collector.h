#ifndef VOLTA_AGENT_SRC_COLLECTORS_RAM_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_RAM_COLLECTOR_H_

#include "collectors/collector.h"

namespace volta {
namespace agent {
namespace collectors {

class RamCollector : public Collector {
public:
    std::vector<Metric> Collect() override;

private:
    void ReadStats(uint64_t& used, uint64_t& total);

    bool initialized_ = false;
};

} // namespace collectors
} // namespace agent
} // namespace volta

#endif // VOLTA_AGENT_SRC_COLLECTORS_RAM_COLLECTOR_H_
