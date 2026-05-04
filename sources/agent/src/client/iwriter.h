#ifndef VOLTA_AGENT_CLIENT_IWRITER_H
#define VOLTA_AGENT_CLIENT_IWRITER_H

#include <mutex>
#include <queue>

namespace volta {
namespace agent {
namespace client {

template<typename T>
class IWriter {
public:
  virtual ~IWriter() = default;
  virtual void EnqueueWrite(T msg) = 0;

protected:
  virtual void Write() = 0;

  std::queue<T> writerqu_;
  bool writing_ = false;
};

} // namespace client
} // namespace agent
} // namespace volta

#endif // VOLTA_AGENT_CLIENT_IWRITER_H