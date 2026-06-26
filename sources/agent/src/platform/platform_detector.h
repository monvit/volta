#ifndef VOLTA_AGENT_PLATFORM_PLATFORM_DETECTOR_H_
#define VOLTA_AGENT_PLATFORM_PLATFORM_DETECTOR_H_

#include <string>

#include "platform/hardware_info.h"

namespace volta {
namespace agent {
namespace platform {

class PlatformDetector {
 public:
  HardwareInfo Detect();
  void PrintDetectedInfo(const HardwareInfo& info);

 private:
  std::string DetectOS();
};

}  // namespace platform
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_PLATFORM_PLATFORM_DETECTOR_H_
