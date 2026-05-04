#include "volta_collector_client.h"

#include <grpcpp/create_channel.h>

namespace volta {
namespace agent {
namespace client {

Client::Client(std::shared_ptr<grpc::Channel> channel)
  : stub_(::volta::VoltaCollector::NewStub(channel)) {}

// TODO: consider making this more configurable, e.g. support TLS, custom
// options, etc.
std::shared_ptr<grpc::Channel> Client::CreateChannel(const std::string& address) {
  return grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
}

void Client::Connect() {
  connect_reactor_ = std::make_unique<ConnectReactor>(this, stub_.get());
  grpc::Status status = connect_reactor_->Await();

  if (!status.ok()) {
    std::cout << "Stream closed with error: " << status.error_message() << std::endl;
  }
}

void Client::OnMessage(const ::volta::ControlMessage& msg) {
  switch (msg.type()) {
    case ::volta::MessageType::PING: {
      connect_reactor_->EnqueueWrite(connect_reactor_->CreateMessage(::volta::MessageType::PONG));
      break;
    }

    case ::volta::MessageType::PONG: {
      break;
    }

    case ::volta::MessageType::SEND_DATA: {
      SendData();
      break;
    }

    case ::volta::MessageType::STREAM_DATA: {
      StreamData();
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

void Client::SendData() {
  connect_reactor_->EnqueueWrite(connect_reactor_->CreateMessage(::volta::MessageType::OK));
}

void Client::StreamData() {
  stream_data_reactor_ = std::make_unique<StreamDataReactor>(stub_.get(), [this](const grpc::Status& status) {
    if (!status.ok()) {
      std::cerr << "StreamData RPC failed: " << status.error_message() << std::endl;
    } else {
      std::cout << "StreamData RPC completed successfully" << std::endl;
    }
  });

  connect_reactor_->EnqueueWrite(connect_reactor_->CreateMessage(::volta::MessageType::OK));
}

} // namespace client
} // namespace agent
} // namespace volta
