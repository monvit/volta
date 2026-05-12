#ifndef VOLTA_AGENT_SRC_COLLECTORS_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_COLLECTOR_H_

#include <vector>

#include "metric.h"
#include "platform/hardware_info.h"

namespace volta {
namespace agent {
namespace collectors {

class Collector {
 public:
  virtual ~Collector() = default;

  virtual std::vector<Metric> Collect() = 0;

  virtual bool IsSupported() = 0;

  virtual std::vector<v1::MetricType> Satisfiable() = 0;

  virtual bool Init() { return true; }
};

class CollectorRegistry {
 public:
  static CollectorRegistry& Instance() {
    static CollectorRegistry instance;
    return instance;
  }

  void Register(std::unique_ptr<Collector> collector) {
    entries_.push_back(std::move(collector));
  }

  std::vector<Collector*> Resolve(const std::vector<v1::MetricType>& desired,
                                  platform::HardwareInfo hw) const {
    std::vector<Collector*> result;
    for (auto& collector : entries_) {
      if (!collector->IsSupported()) continue;

      for (const auto& type : collector->Satisfiable()) {
        if (std::find(desired.begin(), desired.end(), type) != desired.end()) {
          result.push_back(collector.get());
          break;
        }
      }
    }
    return result;
  }

 private:
  CollectorRegistry() = default;
  std::vector<std::unique_ptr<Collector>> entries_;
};

template <typename Derived>
class RegisteredCollector : public Collector {
  static bool Register() {
    std::unique_ptr<Collector> c = std::make_unique<Derived>();
    CollectorRegistry::Instance().Register(std::move(c));
    return true;
  }
  static inline bool registered_ = Register();

  static constexpr std::integral_constant<const bool*, &registered_> force_{};

 public:
};

}  // namespace collectors
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_COLLECTORS_COLLECTOR_H_
