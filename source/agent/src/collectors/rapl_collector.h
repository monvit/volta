#ifndef VOLTA_AGENT_SRC_COLLECTORS_RAPL_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_RAPL_COLLECTOR_H_

#include "collectors/collector.h"

namespace volta {
namespace agent {
namespace collectors {

class RaplCollector : public Collector {
 public:
  RaplCollector();
  // ~RaplCollector() override;
RaplCollector(const RaplCollector&) = delete;
RaplCollector& operator=(const RaplCollector&) = delete;
  std::vector<Metric> Collect() override;
  ~RaplCollector();

 private:
  uint64_t ReadMSR(uint8_t core, uint32_t offset);
  void OpenMSR();
  void CloseMSR(int fd);
  bool initialized_ = false;
  double power_units_, energy_units_, time_units_;
  std::vector<int> MSR_files_;
  double last_value;

  class MSR_Read_Exception : std::exception {};
  class MSR_Open_Exception : std::exception {};

  struct MSR_RAPL {
    static constexpr uint32_t POWER_UNIT = 0x606;
    struct Units {
      static constexpr uint32_t POWER_UNIT_OFFSET = 0;
      static constexpr uint32_t POWER_UNIT_MASK = 0x0F;
      static constexpr uint32_t ENERGY_UNIT_OFFSET = 0x08;
      static constexpr uint32_t ENERGY_UNIT_MASK = 0x1F00;
      static constexpr uint32_t TIME_UNIT_OFFSET = 0x10;
      static constexpr uint32_t TIME_UNIT_MASK = 0xF000;
    };

    struct PKG {
      static constexpr uint32_t POWER_LIMIT = 0x610;
      static constexpr uint32_t ENERGY_STATUS = 0x611;
      static constexpr uint32_t PERF_STATUS = 0x613;
      static constexpr uint32_t POWER_INFO = 0x614;
    };

    struct PP0 {
      static constexpr uint32_t POWER_LIMIT = 0x638;
      static constexpr uint32_t ENERGY_STATUS = 0x639;
      static constexpr uint32_t POLICY = 0x63A;
      static constexpr uint32_t PERF_STATUS = 0x63B;
    };

    struct PP1 {
      static constexpr uint32_t POWER_LIMIT = 0x640;
      static constexpr uint32_t ENERGY_STATUS = 0x641;
      static constexpr uint32_t POLICY = 0x642;
    };

    struct DRAM {
      static constexpr uint32_t POWER_LIMIT = 0x618;
      static constexpr uint32_t ENERGY_STATUS = 0x619;
      static constexpr uint32_t PERF_STATUS = 0x61B;
      static constexpr uint32_t POWER_INFO = 0x61C;
    };
  };
};

}  // namespace collectors
}  // namespace agent
}  // namespace volta
#endif
