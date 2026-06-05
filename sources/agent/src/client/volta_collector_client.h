#ifndef VOLTA_AGENT_CLIENT_VOLTA_COLLECTOR_CLIENT_H_
#define VOLTA_AGENT_CLIENT_VOLTA_COLLECTOR_CLIENT_H_

#include <chrono>
#include <filesystem>
#include <memory>

#include "config/config.h"
#include "connect_reactor.h"
#include "imessage_handler.h"
#include "stream_data_reactor.h"
#include "volta.grpc.pb.h"

namespace volta {
namespace agent {
namespace client {

class Client : public IMessageHandler {
 public:
  Client(std::shared_ptr<grpc::Channel> channel, config::Config& config);
  ~Client() override = default;

  void Connect();
  void OnMessage(
      const ::volta::ControlMessage& msg) override;  // IMessageHandler

  static std::shared_ptr<grpc::Channel> CreateChannel(
      const std::string& address);

 private:
  void SendData();
  void StreamData();

  bool LoadUUID(std::string& out_uuid);
  void SaveUUID(const std::string& uuid);

  std::string id_ = "";
  config::Config& config_;

  std::unique_ptr<::volta::VoltaCollector::Stub> stub_;
  std::unique_ptr<ConnectReactor> connect_reactor_;
  std::unique_ptr<StreamDataReactor> stream_data_reactor_;

  static std::filesystem::path kUUIDFile;
};

}  // namespace client
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CLIENT_VOLTA_COLLECTOR_CLIENT_H_