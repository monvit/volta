#include "volta_collector_client.h"

#include <grpcpp/create_channel.h>

#include <fstream>

#include "buffer.h"

namespace volta {
namespace agent {
namespace client {

std::filesystem::path Client::kUUIDFile = "agent.uuid";

Client::Client(std::shared_ptr<grpc::Channel> channel, config::Config& config,
               std::shared_ptr<::volta::agent::MetricsBuffer> buffer)
    : stub_(::volta::VoltaCollector::NewStub(channel)),
      config_(config),
      buffer_(buffer) {}

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
    connect_reactor_->EnqueueWrite(
        connect_reactor_->CreateMessage(::volta::MessageType::MESSAGE_ERROR,
                                        "Empty UUID received from server"));
    return;
  } else if (std::filesystem::exists(kUUIDFile)) {
    std::cerr << "UUID file already exists, refusing to overwrite" << std::endl;
    connect_reactor_->EnqueueWrite(
        connect_reactor_->CreateMessage(::volta::MessageType::MESSAGE_ERROR,
                                        "UUID file already exists on agent"));
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
  std::cout << "Received message from server: " << msg.DebugString()
            << std::endl;

  switch (msg.type()) {
    // TODO: move pinging to grpc server itself
    case ::volta::MessageType::MESSAGE_PING: {
      // TODO: timeout if pong not sent within certain time?
      connect_reactor_->EnqueueWrite(
          connect_reactor_->CreateMessage(::volta::MessageType::MESSAGE_PONG));
      break;
    }

    case ::volta::MessageType::MESSAGE_PONG: {
      // TODO: monitor latency?
      break;
    }

    case ::volta::MessageType::MESSAGE_SEND_DATA: {
      SendData();
      break;
    }

    case ::volta::MessageType::MESSAGE_STREAM_DATA: {
      StreamData();
      break;
    }

    case ::volta::MessageType::MESSAGE_ERROR: {
      std::cerr << "Received error message from server" << std::endl;
      break;
    }

    case ::volta::MessageType::MESSAGE_OK: {
      break;
    }

    case ::volta::MessageType::MESSAGE_ID: {
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
      connect_reactor_->CreateMessage(::volta::MessageType::MESSAGE_OK));
}

void Client::StreamData() {
  if (buffer_ == nullptr) {
    std::cerr << "Cannot stream data: buffer is null" << std::endl;
    connect_reactor_->EnqueueWrite(connect_reactor_->CreateMessage(
        ::volta::MessageType::MESSAGE_ERROR, "Metrics buffer is null"));
    return;
  }

  stream_data_reactor_ = std::make_unique<StreamMetricsReactor>(
      stub_.get(), id_, buffer_, [this](const grpc::Status& status) {
        if (!status.ok()) {
          std::cerr << "StreamData RPC failed: " << status.error_message()
                    << std::endl;
        } else {
          std::cout << "StreamData RPC completed successfully" << std::endl;
        }
      });

  connect_reactor_->EnqueueWrite(
      connect_reactor_->CreateMessage(::volta::MessageType::MESSAGE_OK));
}

}  // namespace client
}  // namespace agent
}  // namespace volta
