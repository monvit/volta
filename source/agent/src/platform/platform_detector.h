#ifndef VOLTA_AGENT_PLATFORM_PLATFORM_DETECTOR_H_
#define VOLTA_AGENT_PLATFORM_PLATFORM_DETECTOR_H_

#include <string>
#include <vector>
#include <cstdint>

#include "platform/hardware_info.h"

namespace volta {
namespace agent {
namespace platform {

class PlatformDetector {
public:
    HardwareInfo Detect();
};

} // namespace platform
} // namespace agent
} // namespace volta

#endif  // VOLTA_AGENT_PLATFORM_PLATFORM_DETECTOR_H_
