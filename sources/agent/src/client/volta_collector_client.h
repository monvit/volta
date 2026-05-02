#include "imessage_handler.h"
#include "readerwriter.h"
#include "volta.grpc.pb.h"

#include <memory>

namespace volta {
namespace agent {
namespace client {

class Client : public IMessageHandler {
public:
  Client(std::shared_ptr<grpc::Channel> channel);
  ~Client() override = default;

  void Connect();
  void OnMessage(const ::volta::ControlMessage& msg) override;

  static std::shared_ptr<grpc::Channel> CreateChannel(const std::string& address);

private:
  std::unique_ptr<::volta::VoltaCollector::Stub> stub_;
  std::unique_ptr<ReaderWriter> rw_;
};

} // namespace client
} // namespace agent
} // namespace volta