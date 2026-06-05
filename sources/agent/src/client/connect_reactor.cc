#include "connect_reactor.h"

#include <iostream>

#include "imessage_handler.h"

namespace volta {
namespace agent {
namespace client {

ConnectReactor::ConnectReactor(IMessageHandler* handler, const std::string& id,
                               ::volta::VoltaCollector::Stub* stub)
    : handler_(handler) {
  context_.AddMetadata("agent-id", id);
  stub->async()->Connect(&context_, this);

  EnqueueWrite(CreateMessage(::volta::MessageType::MESSAGE_ID));

  StartRead(&res_);
  StartCall();
}

void ConnectReactor::OnWriteDone(bool ok) {
  if (!ok) {
    std::cerr << "Write failed" << std::endl;
    return;
  }

  std::lock_guard l(mu_);

  writerqu_.pop();
  writing_ = false;
  std::cout << "[" << std::chrono::system_clock::now() << "]"
            << " Finished writing message to server, queue size: "
            << writerqu_.size() << std::endl;

  Write();
}

void ConnectReactor::OnReadDone(bool ok) {
  if (!ok) {
    std::cerr << "Read failed" << std::endl;
    return;
  }

  std::cout << "[" << std::chrono::system_clock::now() << "]"
            << " Message from server: "
            << ::volta::MessageType_Name(res_.type()) << std::endl;
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
  cv_.wait(l, [this] { return done_; });

  return std::move(status_);
}

void ConnectReactor::EnqueueWrite(::volta::ControlMessage msg) {
  std::cout << "[" << std::chrono::system_clock::now() << "]"
            << " Enqueuing message to server: "
            << ::volta::MessageType_Name(msg.type()) << std::endl;

  std::lock_guard l(mu_);

  writerqu_.push(std::move(msg));
  Write();
}

// Must be called with wmu_ locked.
void ConnectReactor::Write() {
  if (writing_ || writerqu_.empty()) {
    return;
  }

  std::cout << "[" << std::chrono::system_clock::now() << "]"
            << " Starting write of message to server: "
            << ::volta::MessageType_Name(writerqu_.front().type()) << std::endl;

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
