#ifndef VOLTA_AGENT_SRC_BUFFER_H_
#define VOLTA_AGENT_SRC_BUFFER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "config/config.h"
#include "metric.h"

namespace volta {
namespace agent {

struct Sample {
  int64_t timestamp_ns = 0;
  double value = 0.0;
};

struct BufferKey {
  int32_t metric_type = 0;

  // GPU fields (valid when metric_type in [200, 299])
  uint16_t pci_domain = 0;
  uint8_t pci_bus = 0;
  uint8_t pci_device = 0;
  uint8_t pci_function = 0;

  // CPU fields (valid when metric_type in [100, 199])
  uint8_t socket_index = 0;
  uint16_t core_index = 0;

  // Network fields (valid when metric_type in [500, 599])
  uint32_t ifindex = 0;

  // Disk fields (valid when metric_type in [400, 499])
  uint32_t disk_major = 0;
  uint32_t disk_minor = 0;

  bool operator==(const BufferKey&) const = default;
};

struct BufferKeyHash {
  size_t operator()(const BufferKey& key) const noexcept;
};

class SeriesBuffer {
 public:
  explicit SeriesBuffer(size_t capacity = 0);

  void SetCapacity(size_t capacity);
  size_t Capacity() const { return capacity_; }
  size_t Size() const { return samples_.size(); }
  bool Empty() const { return samples_.empty(); }

  void Push(const Sample& sample);
  std::optional<Sample> Latest() const;
  std::vector<Sample> Snapshot() const;

 private:
  size_t capacity_ = 0;
  std::vector<Sample> samples_;
  size_t head_ = 0;
  bool wrapped_ = false;
};

static_assert(std::is_standard_layout_v<BufferKey>);

class MetricsBuffer {
 public:
  explicit MetricsBuffer(const config::Config& cfg);

  void SetCapacityPerSeries(size_t capacity);
  size_t CapacityPerSeries() const { return capacity_per_series_; }

  void AddSample(const BufferKey& key, const Sample& sample);
  void AddMetric(const Metric& metric);

  std::optional<Sample> Latest(const BufferKey& key) const;
  std::vector<std::pair<BufferKey, Sample>> LatestSamples() const;

 private:
  static BufferKey MakeBufferKey(const Metric& metric);

  size_t capacity_per_series_ = 0;
  std::unordered_map<BufferKey, SeriesBuffer, BufferKeyHash> series_;
};

}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_BUFFER_H_
