#include "volta_collector_client.h"

#include <grpcpp/create_channel.h>

namespace volta {
namespace agent {
namespace client {

Client::Client(std::shared_ptr<grpc::Channel> channel)
  : stub_(::volta::VoltaCollector::NewStub(channel)) {}

// TODO: consider making this more configurable, e.g. support TLS, custom options, etc.
std::shared_ptr<grpc::Channel> Client::CreateChannel(const std::string& address) {
  return grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
}

void Client::Connect() {
  rw_ = std::make_unique<ReaderWriter>(this, stub_.get());
  grpc::Status status = rw_->Await();

  if (!status.ok()) {
    std::cout << "Stream closed with error: " << status.error_message() << std::endl;
  }
}

void Client::OnMessage(const ::volta::ControlMessage& msg) {
  switch (msg.type()) {
    case ::volta::MessageType::PING: {
      rw_->EnqueueWrite(::volta::MessageType::PONG);
      break;
    }

    case ::volta::MessageType::PONG: {
      break;
    }

    case ::volta::MessageType::SEND_DATA: {
      rw_->EnqueueWrite(::volta::MessageType::OK);
      break;
    }

    case ::volta::MessageType::ERROR: {
      std::cerr << "Received error message from server" << std::endl;
      break;
    }

    case ::volta::MessageType::OK: {
      break;
    }

    default: {
      std::cerr << "Received unknown message type from server" << std::endl;
      break;
    }
  }
}

} // namespace client
} // namespace agent
} // namespace volta
