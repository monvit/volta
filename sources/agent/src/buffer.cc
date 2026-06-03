#include "buffer.h"

#include <cstring>
#include <shared_mutex>
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

SeriesBuffer::SeriesBuffer(size_t capacity) : mutex_() {
  SetCapacity(capacity);
}

void SeriesBuffer::SetCapacity(size_t capacity) {
  capacity_ = capacity;
  samples_.clear();
  samples_.reserve(capacity_);
  samples_.resize(capacity_);
  head_ = 0;
  wrapped_ = false;
}

void SeriesBuffer::Push(const Sample& sample) {
  std::lock_guard lock(mutex_);
  if (capacity_ == 0) return;

  if (head_ - tail_ >= capacity_) tail_++;  // drop oldest unsent

  samples_[head_ % capacity_] = sample;
  head_++;
}

std::optional<Sample> SeriesBuffer::Latest() const {
  std::lock_guard lock(mutex_);

  if (samples_.empty()) return std::nullopt;
  return samples_[(head_ - 1) % capacity_];
}

SeriesBuffer::Snapshot SeriesBuffer::GetSnapshot() const {
  std::lock_guard lock(mutex_);

  if (samples_.empty()) return {};

  std::vector<Sample> result;
  result.reserve(samples_.size());

  for (size_t i = tail_; i < head_; ++i) {
    result.push_back(samples_[i % capacity_]);
  }

  return {result, head_};
}

void SeriesBuffer::AckSnapshot(SeriesBuffer::Snapshot snapshot) {
  std::lock_guard lock(mutex_);

  if (snapshot.end > tail_) tail_ = snapshot.end;
}

MetricsBuffer::MetricsBuffer(const config::Config& cfg)
    : mutex_(),
      capacity_per_series_(cfg.buffered_time_window / cfg.collection_interval) {
}

void MetricsBuffer::SetCapacityPerSeries(size_t capacity) {
  std::lock_guard lock(mutex_);
  capacity_per_series_ = capacity;
  for (auto& [_, buffer] : series_) {
    buffer->SetCapacity(capacity_per_series_);
  }
}

SeriesBuffer* MetricsBuffer::GetBuffer(const BufferKey& key) {
  std::shared_lock lock(mutex_);  // read-only, no insertion
  auto it = series_.find(key);
  return it != series_.end() ? &*it->second : nullptr;
}

void MetricsBuffer::AddSample(const BufferKey& key, const Sample& sample) {
  auto [it, inserted] = series_.emplace(key, nullptr);
  if (inserted) {
    it->second = std::make_unique<SeriesBuffer>(capacity_per_series_);
    keys_.push_back(key);
  }
  it->second->Push(sample);
}

void MetricsBuffer::AddMetric(const Metric& metric) {
  if (metric.type == MetricType::METRIC_TYPE_UNSPECIFIED) return;
  AddSample(MakeBufferKey(metric),
            Sample{.timestamp_ns = metric.timestamp, .value = metric.value});
}

void MetricsBuffer::AddMetrics(const std::vector<Metric>& metrics) {
  std::lock_guard lock(mutex_);
  for (auto metric : metrics) AddMetric(metric);
}

std::optional<Sample> MetricsBuffer::Latest(const BufferKey& key) const {
  std::lock_guard lock(mutex_);

  auto it = series_.find(key);
  if (it == series_.end()) return std::nullopt;
  return it->second->Latest();
}

std::vector<std::pair<BufferKey, Sample>> MetricsBuffer::LatestSamples() const {
  std::lock_guard lock(mutex_);

  std::vector<std::pair<BufferKey, Sample>> result;
  result.reserve(series_.size());
  for (const auto& [key, series] : series_) {
    auto latest = series->Latest();
    if (latest.has_value()) {
      result.emplace_back(key, *latest);
    }
  }
  return result;
}

std::vector<BufferKey> MetricsBuffer::GetAllKeys() const {
  std::lock_guard lock(mutex_);
  return keys_;
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
