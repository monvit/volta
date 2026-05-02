#include "imessage_handler.h"
#include "readerwriter.h"

#include <iostream>

namespace volta {
namespace agent {
namespace client {

ReaderWriter::ReaderWriter(IMessageHandler* handler, ::volta::VoltaCollector::Stub* stub)
  : handler_(handler) {
  stub->async()->Connect(&context_, this);

  EnqueueWrite(::volta::MessageType::PING);

  StartRead(&res_);
  StartCall();
}

void ReaderWriter::OnWriteDone(bool ok) {
  if (!ok) {
    std::cerr << "Write failed" << std::endl;
    return;
  }

  std::lock_guard l(mu_);

  wqu_.pop();
  writing_ = false;
  std::cout << "[" << std::chrono::system_clock::now() << "]"
            << " Finished writing message to server, queue size: " << wqu_.size() << std::endl;

  TryStartWrite();
}

void ReaderWriter::OnReadDone(bool ok) {
  if (!ok) {
    std::cerr << "Read failed" << std::endl;
    return;
  }

  std::cout << "[" << std::chrono::system_clock::now() << "]"
            << " Message from server: " << ::volta::MessageType_Name(res_.type()) << std::endl;
  handler_->OnMessage(res_);
  StartRead(&res_);
}

void ReaderWriter::OnDone(const grpc::Status& status) {
  std::unique_lock<std::mutex> l(mu_);
  status_ = status;
  done_ = true;
  cv_.notify_one();
}

grpc::Status ReaderWriter::Await() {
  std::unique_lock<std::mutex> l(mu_);
  cv_.wait(l, [this] { return done_; });

  return std::move(status_);
}

void ReaderWriter::EnqueueWrite(::volta::MessageType type, const std::string& error) {
  std::cout << "[" << std::chrono::system_clock::now() << "]"
            << " Enqueuing message to server: " << ::volta::MessageType_Name(type) << std::endl;

  std::lock_guard l(mu_);

  ::volta::ControlMessage cm;
  cm.set_type(type);

  if (type == ::volta::MessageType::ERROR) {
    cm.set_error(error);
  }

  wqu_.push(std::move(cm));
  TryStartWrite();
}

// Must be called under a lock
void ReaderWriter::TryStartWrite() {
  if (writing_ || wqu_.empty()) {
    return;
  }

  std::cout << "[" << std::chrono::system_clock::now() << "]"
            << " Starting write of message to server: " << ::volta::MessageType_Name(wqu_.front().type()) << std::endl;

  writing_ = true;
  auto& msg = wqu_.front();
  StartWrite(&msg);
}

} // namespace client
} // namespace agent
} // namespace volta
