#include "volta_collector_client.h"

#include <grpcpp/create_channel.h>

#include <fstream>

#include "buffer.h"

namespace volta {
namespace agent {
namespace client {

std::filesystem::path Client::kUUIDFile = "agent.uuid";

Client::Client(std::shared_ptr<grpc::Channel> channel, config::Config& config,
               std::shared_ptr<::volta::agent::MetricsBuffer> buffer,
               std::unique_ptr<logging::Logger> logger)
    : stub_(::volta::VoltaCollector::NewStub(channel)),
      config_(config),
      buffer_(buffer),
      logger_(std::move(logger)) {}

// TODO: consider making this more configurable, e.g. support TLS, custom
// options, etc.
std::shared_ptr<grpc::Channel> Client::CreateChannel(
    const std::string& address) {
  auto channel =
      grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
  if (!channel) {
    return nullptr;
  }

  // TODO: consider making timeout configurable
  if (!channel->WaitForConnected(std::chrono::system_clock::now() +
                                 std::chrono::seconds(5))) {
    return nullptr;
  }

  return channel;
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
    logger_->Log(LogLevel::ERROR_LEVEL, "Cannot save empty UUID");
    connect_reactor_->EnqueueWrite(
        connect_reactor_->CreateMessage(::volta::MessageType::MESSAGE_ERROR,
                                        "Empty UUID received from server"));
    return;
  } else if (std::filesystem::exists(kUUIDFile)) {
    logger_->Log(LogLevel::ERROR_LEVEL,
                 "UUID file already exists, refusing to overwrite");
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

  connect_reactor_ = std::make_unique<ConnectReactor>(
      this, id_, stub_.get(), logger_->WithName("ConnectReactor"));
  grpc::Status status = connect_reactor_->Await();

  if (!status.ok()) {
    logger_->Log(LogLevel::ERROR_LEVEL, "Stream closed with error: {}",
                 status.error_message());
  }
}

void Client::OnMessage(const ::volta::ControlMessage& msg) {
  logger_->Log(LogLevel::DEBUG_LEVEL, "Received message from server: {}",
               msg.DebugString());

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
      logger_->Log(LogLevel::ERROR_LEVEL, "Received error message from server");
      break;
    }

    case ::volta::MessageType::MESSAGE_OK: {
      break;
    }

    case ::volta::MessageType::MESSAGE_ID: {
      logger_->Log(LogLevel::INFO_LEVEL, "Received ID from server: {}",
                   msg.payload());
      SaveUUID(msg.payload());
      break;
    }

    default: {
      logger_->Log(LogLevel::ERROR_LEVEL,
                   "Received unknown message type from server");
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
    logger_->Log(LogLevel::ERROR_LEVEL, "Cannot stream data: buffer is null");
    connect_reactor_->EnqueueWrite(connect_reactor_->CreateMessage(
        ::volta::MessageType::MESSAGE_ERROR, "Metrics buffer is null"));
    return;
  }

  if (stream_data_reactor_ != nullptr) {
    logger_->Log(LogLevel::ERROR_LEVEL,
                 "StreamData RPC already active, refusing to start another");
    connect_reactor_->EnqueueWrite(connect_reactor_->CreateMessage(
        ::volta::MessageType::MESSAGE_ERROR, "StreamData RPC already active"));
    return;
  }

  stream_data_reactor_ = std::make_unique<StreamMetricsReactor>(
      stub_.get(), id_, buffer_,
      [this](const grpc::Status& status) {
        if (!status.ok()) {
          logger_->Log(LogLevel::ERROR_LEVEL, "StreamData RPC failed: {}",
                       status.error_message());
        } else {
          logger_->Log(LogLevel::NOISY_LEVEL,
                       "StreamData RPC completed successfully");
        }
      },
      logger_->WithName("StreamMetricsReactor"));

  connect_reactor_->EnqueueWrite(
      connect_reactor_->CreateMessage(::volta::MessageType::MESSAGE_OK));
}

}  // namespace client
}  // namespace agent
}  // namespace volta
