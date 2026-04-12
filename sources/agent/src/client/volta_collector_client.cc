#include "volta_collector_client.h"

#include <grpcpp/create_channel.h>

#include "readerwriter.h"

namespace volta {
namespace agent {
namespace client {
VoltaCollectorClient::VoltaCollectorClient(
    std::shared_ptr<grpc::Channel> channel)
    : stub_(::volta::VoltaCollector::NewStub(channel)) {}

void VoltaCollectorClient::Connect() {
  ReaderWriter rw(stub_.get());
  grpc::Status status = rw.Await();

  if (!status.ok()) {
    std::cout << "Stream closed with error: " << status.error_message()
              << std::endl;
  }
}
std::shared_ptr<grpc::Channel> VoltaCollectorClient::CreateChannel(
    const std::string& address) {
  return grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
}
}  // namespace client
}  // namespace agent
}  // namespace volta
