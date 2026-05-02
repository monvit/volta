#include "volta.grpc.pb.h"

#include <condition_variable>
#include <mutex>
#include <queue>

namespace volta {
namespace agent {
namespace client {

class IMessageHandler;

class ReaderWriter : public grpc::ClientBidiReactor<::volta::ControlMessage, ::volta::ControlMessage> {
public:
  explicit ReaderWriter(IMessageHandler* handler, ::volta::VoltaCollector::Stub* stub);
  ~ReaderWriter() = default;

  void OnWriteDone(bool ok) override;
  void OnReadDone(bool ok) override;
  void OnDone(const grpc::Status& status) override;
  void EnqueueWrite(::volta::MessageType type, const std::string& error = "");

  grpc::Status Await();

private:
  void TryStartWrite();

  IMessageHandler* handler_;
  ::volta::VoltaCollector::Stub* stub_;
  grpc::ClientContext context_;

  std::queue<::volta::ControlMessage> wqu_;
  bool writing_ = false;

  ::volta::ControlMessage res_;

  grpc::Status status_;
  std::condition_variable cv_;
  std::mutex mu_;
  bool done_ = false;
};

} // namespace client
} // namespace agent
} // namespace volta