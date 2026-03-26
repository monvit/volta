#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

#include "volta.grpc.pb.h"

namespace volta {
namespace agent {
namespace client {

class ReaderWriter : grpc::ClientBidiReactor<::volta::ControlMessage,
                                             ::volta::ControlMessage> {
 public:
  ReaderWriter(VoltaCollector::Stub* stub) {
    stub->async()->Connect(&context_, this);
    StartRead(&read_);
    StartCall();
  }

  void OnReadDone(bool ok) override {
    if (!ok) {
      std::cout << "Error while reading" << std::endl;
      return;
    }

    std::cout << read_.payload() << " " << read_.type() << std::endl;

    write_.set_payload("ack: " + read_.payload());
    write_.set_type(::volta::MessageType::OK);
    StartWrite(&write_);
  }

  void OnWriteDone(bool ok) override {
    if (!ok) {
      std::cout << "Error while writing" << std::endl;
    }

    StartRead(&read_);
  }

  void OnDone(const grpc::Status& status) override {
    std::unique_lock<std::mutex> l(mu_);
    status_ = status;
    done_ = true;
    cv_.notify_one();
  }

  grpc::Status Await() {
    std::unique_lock<std::mutex> l(mu_);
    cv_.wait(l, [this] { return done_; });
    return std::move(status_);
  }

 private:
  grpc::ClientContext context_;
  ::volta::ControlMessage read_;
  ::volta::ControlMessage write_;
  std::mutex mu_;
  std::condition_variable cv_;
  grpc::Status status_;
  bool done_ = false;
};

}  // namespace client
}  // namespace agent
}  // namespace volta