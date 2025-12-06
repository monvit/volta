#ifndef VOLTA_AGENT_SRC_METRIC_H
#define VOLTA_AGENT_SRC_METRIC_H

#include <cstdint>
#include <string>

namespace volta {
namespace agent {

struct Metric {
  std::string name;
  double value;
  int64_t timestamp;
};

}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_METRIC_H
