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