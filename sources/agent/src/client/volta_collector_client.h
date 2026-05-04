#ifndef VOLTA_AGENT_CLIENT_VOLTA_COLLECTOR_CLIENT_H_
#define VOLTA_AGENT_CLIENT_VOLTA_COLLECTOR_CLIENT_H_

#include <chrono>
#include <memory>

#include "connect_reactor.h"
#include "imessage_handler.h"
#include "stream_data_reactor.h"
#include "volta.grpc.pb.h"

namespace volta {
namespace agent {
namespace client {

class Client : public IMessageHandler {
public:
  Client(std::shared_ptr<grpc::Channel> channel);
  ~Client() override = default;

  void Connect();
  void OnMessage(const ::volta::ControlMessage& msg) override; // IMessageHandler

  static std::shared_ptr<grpc::Channel> CreateChannel(const std::string& address);

private:
  void SendData();
  void StreamData();

  std::unique_ptr<::volta::VoltaCollector::Stub> stub_;
  std::unique_ptr<ConnectReactor> connect_reactor_;
  std::unique_ptr<StreamDataReactor> stream_data_reactor_;
};

} // namespace client
} // namespace agent
} // namespace volta

#endif // VOLTA_AGENT_CLIENT_VOLTA_COLLECTOR_CLIENT_H_