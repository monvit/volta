#include <memory>

#include "volta.grpc.pb.h"

namespace volta {
namespace agent {
namespace client {

class VoltaCollectorClient {
 public:
  VoltaCollectorClient(std::shared_ptr<grpc::Channel> channel);
  ~VoltaCollectorClient() = default;

  void Connect();

  static std::shared_ptr<grpc::Channel> CreateChannel(
      const std::string& address);

 private:
  std::unique_ptr<::volta::VoltaCollector::Stub> stub_;
};

}  // namespace client
}  // namespace agent
}  // namespace volta