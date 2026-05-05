#ifndef VOLTA_AGENT_UTILS_UTILS_H_
#define VOLTA_AGENT_UTILS_UTILS_H_

#include <string>

namespace volta {
namespace agent {
namespace utils {

void PrintCurrentAffinity();
bool IsValidIP(const std::string& ip);
bool IsResolvable(const std::string& host);
std::string GenerateUUIDv4();

}  // namespace utils
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_UTILS_UTILS_H_
