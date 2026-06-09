#ifndef VOLTA_AGENT_CLIENT_CONNECT_REACTOR_H_
#define VOLTA_AGENT_CLIENT_CONNECT_REACTOR_H_

#include <condition_variable>
#include <mutex>
#include <queue>

#include "iwriter.h"
#include "volta.grpc.pb.h"

namespace volta {
namespace agent {
namespace client {

class IMessageHandler;

class ConnectReactor : public grpc::ClientBidiReactor<::volta::ControlMessage,
                                                      ::volta::ControlMessage>,
                       public IWriter<::volta::ControlMessage> {
 public:
  explicit ConnectReactor(IMessageHandler* handler, const std::string& id,
                          ::volta::VoltaCollector::Stub* stub);
  ~ConnectReactor() override {
    std::cout << "ConnectReactor destroyed for agent "
              << context_.GetServerInitialMetadata().find("agent-id")->second
              << std::endl;
  }

  // ClientBidiReactor
  void OnWriteDone(bool ok) override;
  void OnReadDone(bool ok) override;
  void OnDone(const grpc::Status& status) override;

  // IWriter
  void EnqueueWrite(::volta::ControlMessage msg) override;

  ::volta::ControlMessage CreateMessage(::volta::MessageType type,
                                        const std::string& payload = "");
  grpc::Status Await();

 private:
  void Write() override;  // IWriter

  IMessageHandler* handler_;
  ::volta::VoltaCollector::Stub* stub_;
  grpc::ClientContext context_;

  ::volta::ControlMessage res_;

  std::mutex mu_;
  grpc::Status status_;
  std::condition_variable cv_;
  bool done_ = false;
};

}  // namespace client
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CLIENT_CONNECT_REACTOR_H_