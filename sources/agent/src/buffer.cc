#include "buffer.h"

#include <cstring>
#include <functional>
#include <type_traits>
#include <variant>

namespace volta {
namespace agent {

namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

uint64_t HashCombineBytes(uint64_t hash, const void* data, size_t size) {
  const auto* bytes = static_cast<const unsigned char*>(data);
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= kFnvPrime;
  }
  return hash;
}

}  // namespace

size_t BufferKeyHash::operator()(const BufferKey& key) const noexcept {
  uint64_t hash = kFnvOffset;
  hash = HashCombineBytes(hash, &key.metric_type, sizeof(key.metric_type));
  hash = HashCombineBytes(hash, &key.pci_domain, sizeof(key.pci_domain));
  hash = HashCombineBytes(hash, &key.pci_bus, sizeof(key.pci_bus));
  hash = HashCombineBytes(hash, &key.pci_device, sizeof(key.pci_device));
  hash = HashCombineBytes(hash, &key.pci_function, sizeof(key.pci_function));
  hash = HashCombineBytes(hash, &key.socket_index, sizeof(key.socket_index));
  hash = HashCombineBytes(hash, &key.core_index, sizeof(key.core_index));
  hash = HashCombineBytes(hash, &key.ifindex, sizeof(key.ifindex));
  hash = HashCombineBytes(hash, &key.disk_major, sizeof(key.disk_major));
  hash = HashCombineBytes(hash, &key.disk_minor, sizeof(key.disk_minor));
  return static_cast<size_t>(hash);
}

SeriesBuffer::SeriesBuffer(size_t capacity) { SetCapacity(capacity); }

void SeriesBuffer::SetCapacity(size_t capacity) {
  capacity_ = capacity;
  samples_.clear();
  samples_.reserve(capacity_);
  head_ = 0;
  wrapped_ = false;
}

void SeriesBuffer::Push(const Sample& sample) {
  if (capacity_ == 0) return;

  if (samples_.size() < capacity_) {
    samples_.push_back(sample);
    if (samples_.size() == capacity_) {
      head_ = 0;
      wrapped_ = true;
    }
    return;
  }

  samples_[head_] = sample;
  head_ = (head_ + 1) % capacity_;
  wrapped_ = true;
}

std::optional<Sample> SeriesBuffer::Latest() const {
  if (samples_.empty()) return std::nullopt;
  if (!wrapped_) return samples_.back();
  const size_t index = (head_ + capacity_ - 1) % capacity_;
  return samples_[index];
}

std::vector<Sample> SeriesBuffer::Snapshot() const {
  if (samples_.empty()) return {};
  if (!wrapped_) return samples_;

  std::vector<Sample> result;
  result.reserve(samples_.size());
  for (size_t i = 0; i < samples_.size(); ++i) {
    const size_t index = (head_ + i) % capacity_;
    result.push_back(samples_[index]);
  }
  return result;
}

MetricsBuffer::MetricsBuffer(size_t capacity_per_series)
    : capacity_per_series_(capacity_per_series) {}

void MetricsBuffer::SetCapacityPerSeries(size_t capacity) {
  capacity_per_series_ = capacity;
  for (auto& [_, buffer] : series_) {
    buffer.SetCapacity(capacity_per_series_);
  }
}

void MetricsBuffer::AddSample(const BufferKey& key, const Sample& sample) {
  auto& series = series_[key];
  if (series.Capacity() != capacity_per_series_) {
    series.SetCapacity(capacity_per_series_);
  }
  series.Push(sample);
}

void MetricsBuffer::AddMetric(const Metric& metric) {
  if (metric.type == MetricType::METRIC_TYPE_UNSPECIFIED) return;
  AddSample(MakeBufferKey(metric),
            Sample{.timestamp_ns = metric.timestamp, .value = metric.value});
}

std::optional<Sample> MetricsBuffer::Latest(const BufferKey& key) const {
  auto it = series_.find(key);
  if (it == series_.end()) return std::nullopt;
  return it->second.Latest();
}

std::vector<std::pair<BufferKey, Sample>> MetricsBuffer::LatestSamples() const {
  std::vector<std::pair<BufferKey, Sample>> result;
  result.reserve(series_.size());
  for (const auto& [key, series] : series_) {
    auto latest = series.Latest();
    if (latest.has_value()) {
      result.emplace_back(key, *latest);
    }
  }
  return result;
}

BufferKey MetricsBuffer::MakeBufferKey(const Metric& metric) {
  BufferKey key;
  key.metric_type = static_cast<int32_t>(metric.type);
  if (!metric.devId.has_value()) return key;

  const auto& device = metric.devId.value();
  std::visit(
      [&](const auto& id) {
        using T = std::decay_t<decltype(id)>;
        if constexpr (std::is_same_v<T, GpuID>) {
          key.pci_domain = static_cast<uint16_t>(id.pci_domain());
          key.pci_bus = static_cast<uint8_t>(id.pci_bus());
          key.pci_device = static_cast<uint8_t>(id.pci_device());
          key.pci_function = static_cast<uint8_t>(id.pci_function());
        } else if constexpr (std::is_same_v<T, CpuID>) {
          key.socket_index = static_cast<uint8_t>(id.socket_index());
          key.core_index = static_cast<uint16_t>(id.core_index());
        } else if constexpr (std::is_same_v<T, NetInterfaceID>) {
          key.ifindex = id.ifindex();
        } else if constexpr (std::is_same_v<T, DiskID>) {
          key.disk_major = id.major();
          key.disk_minor = id.minor();
        }
      },
      device);

  return key;
}

}  // namespace agent
}  // namespace volta
