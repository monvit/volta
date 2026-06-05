#ifndef VOLTA_AGENT_CLIENT_IREADER_H
#define VOLTA_AGENT_CLIENT_IREADER_H

#include <mutex>
#include <unordered_map>

#include "iwriter.h"

namespace volta {
namespace agent {
namespace client {

template <typename K, typename V, typename T>
class IReaderWriter : public IWriter<V> {
 public:
  virtual ~IReaderWriter() = default;
  virtual void BindMessage(const K& key, V msg) = 0;
  virtual void UnbindMessage(const K& key) = 0;
  virtual void EnqueueWrite(V msg) override = 0;

  V* GetMessage(const K& key) {
    std::lock_guard l(mu_);
    auto it = readermap_.find(key);
    if (it != readermap_.end()) {
      return &it->second;
    }
    return nullptr;
  }

 protected:
  std::unordered_map<K, V> readermap_;
  bool reading_ = false;
  T* msg;
  std::mutex mu_;
};

}  // namespace client
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CLIENT_IREADER_H