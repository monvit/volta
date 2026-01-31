#ifndef VOLTA_AGENT_UTILS_UTILS_H_
#define VOLTA_AGENT_UTILS_UTILS_H_

#include <string>

namespace volta {
namespace agent {
namespace utils {

bool IsValidIP(const std::string &ip);
bool IsResolvable(const std::string &host);

} // namespace utils
} // namespace agent
} // namespace volta

#endif // VOLTA_AGENT_UTILS_UTILS_H_
