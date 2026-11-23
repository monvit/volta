#ifndef VOLTA_AGENT_SRC_COLLECTORS_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_COLLECTOR_H_

#include <algorithm>
#include <vector>

#include "logger/logger.h"
#include "metric.h"

namespace volta {
namespace agent {
namespace collectors {

using logging::Logger;

class Collector {
 public:
  virtual ~Collector() = default;

  // Append collected metrics to the caller-owned, reusable output vector.
  virtual void Collect(std::vector<Metric>& metrics) = 0;

  virtual std::vector<MetricType> Satisfiable() = 0;

  // Configure and initialize this collector for the requested metrics.
  // Returns false if no metrics can be collected in the current environment.
  virtual bool Start(const std::vector<MetricType>& metrics) = 0;

  virtual void SetLogger(std::unique_ptr<Logger>&& logger) {
    logger_ = std::move(logger);
  }

  virtual std::string_view GetName() = 0;

 protected:
  std::unique_ptr<Logger> logger_;
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

  std::vector<MetricType> SatisfiableMetrics() const {
    std::vector<MetricType> metrics;
    for (const auto& collector : entries_) {
      for (const auto metric : collector->Satisfiable()) {
        if (std::find(metrics.begin(), metrics.end(), metric) ==
            metrics.end()) {
          metrics.push_back(metric);
        }
      }
    }
    std::sort(metrics.begin(), metrics.end());
    return metrics;
  }

  std::vector<Collector*> Resolve(const std::vector<MetricType>& desired) {
    std::vector<Collector*> result;
    for (auto& collector : entries_) {
      std::vector<MetricType> active;
      for (const auto& type : collector->Satisfiable()) {
        if (std::find(desired.begin(), desired.end(), type) != desired.end()) {
          active.push_back(type);
        }
      }

      if (!active.empty() && collector->Start(active)) {
        result.push_back(collector.get());
      }
    }
    return result;
  }

  void SetLoggers(const Logger& logger) {
    for (auto& collector : entries_) {
      collector->SetLogger(std::move(logger.WithName(collector->GetName())));
    }
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
