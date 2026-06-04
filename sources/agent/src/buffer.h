#ifndef VOLTA_AGENT_SRC_BUFFER_H_
#define VOLTA_AGENT_SRC_BUFFER_H_

#include <metric.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "config/config.h"

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

static_assert(std::is_standard_layout_v<BufferKey>);

struct BufferKeyHash {
  size_t operator()(const BufferKey& key) const noexcept;
};

class SeriesBuffer {
 public:
  explicit SeriesBuffer(size_t capacity = 0);

  using Snapshot = struct {
    std::vector<Sample> samples;
    size_t end;
  };

  void Push(const Sample& sample);
  std::optional<Sample> Latest() const;

  size_t GetNextSnapshotSize() const;
  // Snapshots all of the unsent data
  Snapshot GetSnapshot() const;

  // Marks all the samples from the snapshot as sent
  void AckSnapshot(const Snapshot& snapshot_end);

  void SetCapacity(size_t capacity);
  size_t Capacity() const { return capacity_; }
  size_t Size() const { return samples_.size(); }
  bool Empty() const { return samples_.empty(); }
  size_t GetHead() const { return head_; }
  size_t GetTail() const { return tail_; }

 private:
  mutable std::shared_mutex mutex_;
  size_t capacity_ = 0;
  std::vector<Sample> samples_;
  size_t head_ = 0;
  size_t tail_ = 0;
};

class MetricsBuffer {
 public:
  explicit MetricsBuffer(const config::Config& cfg);

  size_t CapacityPerSeries() const { return capacity_per_series_; }

  void AddMetrics(const std::vector<Metric>& metrics);
  std::shared_ptr<SeriesBuffer> GetBuffer(const BufferKey& key) const;
  std::vector<BufferKey> GetAllKeys() const;
  std::vector<std::pair<BufferKey, Sample>> LatestSamples() const;

  static BufferKey MakeBufferKey(const Metric& metric);

 private:
  using SeriesMap = std::unordered_map<BufferKey, std::shared_ptr<SeriesBuffer>,
                                       BufferKeyHash>;

  void SetCapacityPerSeries(size_t capacity);
  std::optional<Sample> Latest(const BufferKey& key) const;
  void AddSample(const BufferKey& key, const Sample& sample);
  void AddMetric(const Metric& metric);

  mutable std::shared_mutex mutex_;
  size_t capacity_per_series_ = 0;
  SeriesMap series_;
  std::vector<BufferKey> keys_;
};

}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_SRC_BUFFER_H_
