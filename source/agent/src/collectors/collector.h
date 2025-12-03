#ifndef VOLTA_AGENT_SRC_COLLECTORS_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_COLLECTOR_H_

#include <vector>

#include "metric.h"

namespace volta {
namespace agent {
namespace collectors {

class Collector {
public:
    virtual ~Collector() = default;
    
    virtual std::vector<Metric> Collect() = 0;

    virtual bool Init() { return true; }
};

} // namespace collectors
} // namespace agent
} // namespace volta

#endif // VOLTA_AGENT_SRC_COLLECTORS_COLLECTOR_H_
