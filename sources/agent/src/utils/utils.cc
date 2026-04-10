#include "utils.h"

#include <arpa/inet.h>
#include <netdb.h>

#include <iomanip>
#include <random>

namespace volta {
namespace agent {
namespace utils {

bool IsValidIP(const std::string& ip) {
  sockaddr_in sa4{};
  sockaddr_in6 sa6{};

  return inet_pton(AF_INET, ip.c_str(), &sa4.sin_addr) == 1 ||
         inet_pton(AF_INET6, ip.c_str(), &sa6.sin6_addr) == 1;
}

bool IsResolvable(const std::string& host) {
  addrinfo hints{}, *res = nullptr;
  hints.ai_family = AF_UNSPEC;

  bool ok = getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0;
  freeaddrinfo(res);
  return ok;
}

std::string GenerateUUIDv4() {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dist;

  uint64_t a = dist(gen);
  uint64_t b = dist(gen);

  // version 4
  a = (a & 0xffffffffffff0fffULL) | 0x0000000000004000ULL;
  // variant 1 (RFC 4122)
  b = (b & 0x3fffffffffffffffULL) | 0x8000000000000000ULL;

  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(8) << (a >> 32) << "-"
      << std::setw(4) << ((a >> 16) & 0xffff) << "-" << std::setw(4)
      << (a & 0xffff) << "-" << std::setw(4) << (b >> 48) << "-"
      << std::setw(12) << (b & 0x0000ffffffffffffULL);

  return oss.str();
}

}  // namespace utils
}  // namespace agent
}  // namespace volta
