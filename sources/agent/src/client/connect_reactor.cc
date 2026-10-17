#include "connect_reactor.h"

#include <chrono>
#include <csignal>

#include "imessage_handler.h"
#include "logger/logger.h"

extern volatile sig_atomic_t g_running;

namespace volta {
namespace agent {
namespace client {

ConnectReactor::ConnectReactor(IMessageHandler* handler, const std::string& id,
                               ::volta::VoltaCollector::Stub* stub,
                               std::unique_ptr<logging::Logger> logger)
    : handler_(handler), logger_(std::move(logger)), id_(id) {
  logger_->Log(logging::LogLevel::INFO_LEVEL,
               "Starting ConnectReactor for agent {}", id);

  context_.AddMetadata("agent-id", id);
  stub->async()->Connect(&context_, this);

  EnqueueWrite(CreateMessage(::volta::MessageType::MESSAGE_ID));

  StartRead(&res_);
  StartCall();
}

void ConnectReactor::OnWriteDone(bool ok) {
  if (!ok) {
    logger_->Log(logging::LogLevel::ERROR_LEVEL,
                 "ConnectReactor write failed for agent {}", id_);
    return;
  }

  std::lock_guard l(mu_);

  writerqu_.pop();
  writing_ = false;
  logger_->Log(logging::LogLevel::DEBUG_LEVEL,
               "Finished writing message to server, queue size: {}",
               writerqu_.size());

  Write();
}

void ConnectReactor::OnReadDone(bool ok) {
  if (!ok) {
    logger_->Log(logging::LogLevel::ERROR_LEVEL,
                 "ConnectReactor read failed for agent {}", id_);
    return;
  }

  logger_->Log(logging::LogLevel::DEBUG_LEVEL, "Message from server: {}",
               ::volta::MessageType_Name(res_.type()));
  handler_->OnMessage(res_);
  StartRead(&res_);
}

void ConnectReactor::OnDone(const grpc::Status& status) {
  std::unique_lock<std::mutex> l(mu_);
  status_ = status;
  done_ = true;
  cv_.notify_one();
}

grpc::Status ConnectReactor::Await() {
  std::unique_lock<std::mutex> l(mu_);
  while (!done_) {
    if (!g_running) {
      context_.TryCancel();
    }
    cv_.wait_for(l, std::chrono::milliseconds(200), [this] { return done_; });
  }

  return std::move(status_);
}

void ConnectReactor::EnqueueWrite(::volta::ControlMessage msg) {
  logger_->Log(logging::LogLevel::DEBUG_LEVEL,
               "Enqueuing message to server: {}",
               ::volta::MessageType_Name(msg.type()));

  std::lock_guard l(mu_);

  writerqu_.push(std::move(msg));
  Write();
}

// Must be called with wmu_ locked.
void ConnectReactor::Write() {
  if (writing_ || writerqu_.empty()) {
    return;
  }

  logger_->Log(logging::LogLevel::DEBUG_LEVEL,
               "Starting write of message to server: {}",
               ::volta::MessageType_Name(writerqu_.front().type()));

  writing_ = true;
  auto& msg = writerqu_.front();
  StartWrite(&msg);
}

::volta::ControlMessage ConnectReactor::CreateMessage(
    ::volta::MessageType type, const std::string& payload) {
  ::volta::ControlMessage msg;
  msg.set_type(type);

  if (type == ::volta::MessageType::MESSAGE_ERROR) {
    msg.set_payload(payload);
  }

  return msg;
}

}  // namespace client
}  // namespace agent
}  // namespace volta
