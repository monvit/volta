#include <condition_variable>
#include <mutex>

#include "volta.grpc.pb.h"

namespace volta {
namespace agent {
namespace client {

class ReaderWriter : public grpc::ClientBidiReactor<::volta::ControlMessage,
                                                    ::volta::ControlMessage> {
 public:
  ReaderWriter(::volta::VoltaCollector::Stub* stub);
  ~ReaderWriter() = default;

  void OnWriteDone(bool ok) override;
  void OnReadDone(bool ok) override;
  void OnDone(const grpc::Status& status) override;
  grpc::Status Await();

 private:
  void Respond(::volta::MessageType msg_type);

  grpc::ClientContext context_;
  grpc::Status status_;
  ::volta::ControlMessage resp_;
  ::volta::ControlMessage req_;
  std::condition_variable cv_;
  std::mutex mu_;
  bool done_ = false;
  bool free_ = false;
};

}  // namespace client
}  // namespace agent
}  // namespace volta