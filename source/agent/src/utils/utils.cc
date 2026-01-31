#include "utils.h"

#include <arpa/inet.h>
#include <netdb.h>

namespace volta {
namespace agent {
namespace utils {

bool IsValidIP(const std::string &ip) {
    sockaddr_in sa4{};
    sockaddr_in6 sa6{};

    return inet_pton(AF_INET, ip.c_str(), &sa4.sin_addr) == 1 ||
           inet_pton(AF_INET6, ip.c_str(), &sa6.sin6_addr) == 1;
}

bool IsResolvable(const std::string &host) {
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;

    bool ok = getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0;
    freeaddrinfo(res);
    return ok;
}

} // namespace utils
} // namespace agent
} // namespace volta
