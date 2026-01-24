#include "rapl_collector.h"

#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <cmath>

namespace volta {
namespace agent {
namespace collectors {

RaplCollector::RaplCollector() {
  uint64_t readout = ReadMSR(MSR_RAPL::POWER_UNIT);

  power_units_ = pow(0.5, (double)(readout & 0xf));
  energy_units_ = pow(0.5, (double)((readout >> 8) & 0x1f));
  time_units_ = pow(0.5, (double)((readout >> 16) & 0xf));
  readout = ReadMSR(MSR_RAPL::PKG::ENERGY_STATUS);
  last_value = energy_units_ * readout;
}

std::vector<Metric> RaplCollector::Collect() {
  uint64_t readout = ReadMSR(MSR_RAPL::PKG::ENERGY_STATUS);
  double value = energy_units_ * readout;

  // TODO add more metrics
  Metric m;
  m.name = "cpu_energy_usage_total";
  m.value = value - last_value;
  m.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  last_value = value;
  return {m};
}

uint64_t RaplCollector::ReadMSR(uint32_t offset) {
  // TODO support other cores
  int fd = OpenMSR(0);

  uint64_t data;
  // c-like read for thread safety
  if (pread(fd, &data, sizeof data, offset) != sizeof data) {
    return {};
  }

  CloseMSR(fd);
  return data;
}

int RaplCollector::OpenMSR(uint8_t core) {
  std::string path = "/dev/cpu/" + std::to_string(core) + "/msr";
  return open(path.c_str(), O_RDONLY);
}
void RaplCollector::CloseMSR(int fd) { close(fd); }

}  // namespace collectors
}  // namespace agent
}  // namespace volta
