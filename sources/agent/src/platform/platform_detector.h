#ifndef VOLTA_AGENT_PLATFORM_PLATFORM_DETECTOR_H_
#define VOLTA_AGENT_PLATFORM_PLATFORM_DETECTOR_H_

#include <string>

#include "logger/logger.h"
#include "platform/hardware_info.h"

namespace volta {
namespace agent {
namespace platform {

class PlatformDetector {
 public:
  HardwareInfo Detect();
  void LogDetectedInfo(const HardwareInfo& info, logging::Logger& logger) const;

 private:
  std::string DetectOS();
};

}  // namespace platform
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_PLATFORM_PLATFORM_DETECTOR_H_
