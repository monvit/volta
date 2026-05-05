#include "volta_collector_client.h"

#include <grpcpp/create_channel.h>

#include <fstream>

namespace volta {
namespace agent {
namespace client {

std::filesystem::path Client::kUUIDFile = "agent.uuid";

Client::Client(std::shared_ptr<grpc::Channel> channel, config::Config& config)
    : stub_(::volta::VoltaCollector::NewStub(channel)), config_(config) {}

// TODO: consider making this more configurable, e.g. support TLS, custom
// options, etc.
std::shared_ptr<grpc::Channel> Client::CreateChannel(
    const std::string& address) {
  return grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
}

bool Client::LoadUUID(std::string& out_uuid) {
  std::ifstream f(kUUIDFile);
  if (!f.is_open()) {
    return false;
  }

  std::getline(f, out_uuid);
  return !out_uuid.empty();
}

void Client::SaveUUID(const std::string& uuid) {
  if (uuid.empty()) {
    std::cerr << "Cannot save empty UUID" << std::endl;
    connect_reactor_->EnqueueWrite(connect_reactor_->CreateMessage(
        ::volta::MessageType::ERROR, "Empty UUID received from server"));
    return;
  } else if (std::filesystem::exists(kUUIDFile)) {
    std::cerr << "UUID file already exists, refusing to overwrite" << std::endl;
    connect_reactor_->EnqueueWrite(connect_reactor_->CreateMessage(
        ::volta::MessageType::ERROR, "UUID file already exists on agent"));
    return;
  }

  std::filesystem::path tmp = kUUIDFile;
  tmp += ".tmp";

  {
    std::ofstream f(tmp, std::ios::trunc);
    f << uuid;
    f.flush();
  }

  std::filesystem::rename(tmp, kUUIDFile);
  id_ = uuid;
}

void Client::Connect() {
  // TODO: handle case where UUID file exists but is empty or invalid
  LoadUUID(id_);

  connect_reactor_ = std::make_unique<ConnectReactor>(this, id_, stub_.get());
  grpc::Status status = connect_reactor_->Await();

  if (!status.ok()) {
    std::cout << "Stream closed with error: " << status.error_message()
              << std::endl;
  }
}

void Client::OnMessage(const ::volta::ControlMessage& msg) {
  switch (msg.type()) {
    case ::volta::MessageType::PING: {
      // TODO: timeout if pong not sent within certain time?
      connect_reactor_->EnqueueWrite(
          connect_reactor_->CreateMessage(::volta::MessageType::PONG));
      break;
    }

    case ::volta::MessageType::PONG: {
      // TODO: monitor latency?
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

    case ::volta::MessageType::ID: {
      std::cout << "Received ID from server: " << msg.payload() << std::endl;
      SaveUUID(msg.payload());
      break;
    }

    default: {
      std::cerr << "Received unknown message type from server" << std::endl;
      break;
    }
  }
}

void Client::SendData() {
  connect_reactor_->EnqueueWrite(
      connect_reactor_->CreateMessage(::volta::MessageType::OK));
}

void Client::StreamData() {
  stream_data_reactor_ = std::make_unique<StreamDataReactor>(
      stub_.get(), id_, [this](const grpc::Status& status) {
        if (!status.ok()) {
          std::cerr << "StreamData RPC failed: " << status.error_message()
                    << std::endl;
        } else {
          std::cout << "StreamData RPC completed successfully" << std::endl;
        }
      });

  connect_reactor_->EnqueueWrite(
      connect_reactor_->CreateMessage(::volta::MessageType::OK));
}

}  // namespace client
}  // namespace agent
}  // namespace volta
