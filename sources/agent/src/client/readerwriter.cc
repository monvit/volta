#include "readerwriter.h"

#include <iostream>

volta::agent::client::ReaderWriter::ReaderWriter(
    ::volta::VoltaCollector::Stub* stub) {
  stub->async()->Connect(&context_, this);

  req_.set_type(::volta::MessageType::PING);

  StartWrite(&req_);
  StartRead(&resp_);
  StartCall();
}

void volta::agent::client::ReaderWriter::OnWriteDone(bool ok) {
  if (ok) {
    free_ = true;
  }

  // TODO: Error/failure handling
}

void volta::agent::client::ReaderWriter::OnReadDone(bool ok) {
  if (ok) {
    std::cout << "Message from server: " << resp_.payload() << std::endl;
  }

  // TODO: Error/failure handling
}

void volta::agent::client::ReaderWriter::OnDone(const grpc::Status& status) {
  std::unique_lock<std::mutex> l(mu_);
  status_ = status;
  done_ = true;
  cv_.notify_one();
}

grpc::Status volta::agent::client::ReaderWriter::Await() {
  std::unique_lock<std::mutex> l(mu_);
  cv_.wait(l, [this] { return done_; });

  return std::move(status_);
}
