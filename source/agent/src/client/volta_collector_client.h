#include <grpc/grpc.h>
#include <grpcpp/channel.h>

#include <memory>
#include <string>

#include "volta.grpc.pb.h"

namespace volta {
namespace agent {
namespace client {

class VoltaCollectorClient {
 public:
  VoltaCollectorClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(::volta::VoltaCollector::NewStub(channel)) {};

  static std::shared_ptr<grpc::Channel> CreateChannel(const std::string& host);

  // void SendMessage(const std::string &message);
  // void SendMessages();
  // void GetResponses();
  // void Talk();
  void Connect();

 private:
  std::unique_ptr<VoltaCollector::Stub> stub_;
};

}  // namespace client
}  // namespace agent
}  // namespace volta
