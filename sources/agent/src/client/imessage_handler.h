#ifndef VOLTA_AGENT_CLIENT_IMESSAGE_HANDLER_H_
#define VOLTA_AGENT_CLIENT_IMESSAGE_HANDLER_H_

#include "volta.grpc.pb.h"

namespace volta {
namespace agent {
namespace client {

class IMessageHandler {
public:
  virtual ~IMessageHandler() = default;
  virtual void OnMessage(const ::volta::ControlMessage& msg) = 0;
};

} // namespace client
} // namespace agent
} // namespace volta

#endif // VOLTA_AGENT_CLIENT_IMESSAGE_HANDLER_H_