#include "rapl_collector.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace volta {
namespace agent {
namespace collectors {

namespace {

constexpr const char* kPowercapBase = "/sys/class/powercap";
constexpr const char* kEnergyFile = "energy_uj";
constexpr const char* kMaxEnergyRangeFile = "max_energy_range_uj";
constexpr const char* kNameFile = "name";

std::string Trim(std::string value) {
  auto begin =
      std::find_if_not(value.begin(), value.end(),
                       [](unsigned char c) { return std::isspace(c); });
  auto end =
      std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c);
      }).base();
  if (begin >= end) return {};
  return std::string(begin, end);
}

bool ReadFdText(int fd, std::string* out) {
  char buffer[128];
  const ssize_t size = pread(fd, buffer, sizeof(buffer), 0);
  if (size <= 0) return false;
  out->assign(buffer, static_cast<size_t>(size));
  return true;
}

bool ParseUnsigned(const std::string& text, uint64_t* value) {
  const std::string trimmed = Trim(text);
  if (trimmed.empty()) return false;

  uint64_t parsed = 0;
  const auto [ptr, ec] =
      std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), parsed);
  if (ec != std::errc() || ptr != trimmed.data() + trimmed.size()) {
    return false;
  }
  *value = parsed;
  return true;
}

bool ReadUnsignedFile(const std::filesystem::path& path, uint64_t* value) {
  const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) return false;

  std::string text;
  const bool ok = ReadFdText(fd, &text) && ParseUnsigned(text, value);
  close(fd);
  return ok;
}

bool ReadTextFile(const std::filesystem::path& path, std::string* value) {
  const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) return false;

  const bool ok = ReadFdText(fd, value);
  close(fd);
  return ok;
}

std::optional<uint32_t> ParseSocketIndex(const std::string& name) {
  constexpr std::string_view kPrefix = "package-";
  if (!name.starts_with(kPrefix)) return std::nullopt;

  uint64_t index = 0;
  const std::string suffix = name.substr(kPrefix.size());
  if (!ParseUnsigned(suffix, &index)) return std::nullopt;
  if (index > std::numeric_limits<uint32_t>::max()) return std::nullopt;
  return static_cast<uint32_t>(index);
}

struct DiscoveredZone {
  std::filesystem::path path;
  uint32_t socket_index = 0;
};

std::vector<DiscoveredZone> DiscoverZones() {
  std::vector<DiscoveredZone> zones;
  std::error_code ec;
  const std::filesystem::path base{kPowercapBase};
  if (!std::filesystem::exists(base, ec) ||
      !std::filesystem::is_directory(base, ec)) {
    return zones;
  }

  const auto options =
      std::filesystem::directory_options::skip_permission_denied |
      std::filesystem::directory_options::follow_directory_symlink;
  for (std::filesystem::recursive_directory_iterator it(base, options, ec), end;
       it != end && !ec; it.increment(ec)) {
    const auto& entry = *it;
    if (!entry.is_directory(ec) || ec) {
      ec.clear();
      continue;
    }

    const auto path = entry.path();
    const auto name_path = path / kNameFile;
    const auto energy_path = path / kEnergyFile;
    if (!std::filesystem::exists(name_path, ec) ||
        !std::filesystem::exists(energy_path, ec)) {
      ec.clear();
      continue;
    }

    std::string name;
    if (!ReadTextFile(name_path, &name)) {
      ec.clear();
      continue;
    }
    name = Trim(name);

    const auto socket_index = ParseSocketIndex(name);
    if (!socket_index.has_value()) {
      ec.clear();
      continue;
    }

    zones.push_back({path, socket_index.value()});
  }

  std::sort(zones.begin(), zones.end(),
            [](const DiscoveredZone& lhs, const DiscoveredZone& rhs) {
              if (lhs.socket_index != rhs.socket_index)
                return lhs.socket_index < rhs.socket_index;
              return lhs.path.string() < rhs.path.string();
            });

  return zones;
}

std::optional<uint64_t> ReadEnergyUJ(int fd) {
  std::string text;
  uint64_t value = 0;
  if (!ReadFdText(fd, &text) || !ParseUnsigned(text, &value)) {
    return std::nullopt;
  }
  return value;
}

}  // namespace

RaplCollector::RaplCollector() = default;

bool RaplCollector::Init() {
  ClosePowercap();
  if (!OpenPowercap()) {
    initialized_ = false;
    return false;
  }
  initialized_ = true;
  return true;
}

bool RaplCollector::IsSupported() { return !DiscoverZones().empty(); }

void RaplCollector::SetRequestedMetrics(
    const std::vector<MetricType>& metrics) {
  requested_metrics_ = metrics;
}

std::vector<Metric> RaplCollector::Collect() {
  if (!initialized_ || requested_metrics_.empty()) return {};
  if (std::find(requested_metrics_.begin(), requested_metrics_.end(),
                MetricType::METRIC_TYPE_CPU_POWER_PACKAGE) ==
      requested_metrics_.end()) {
    return {};
  }

  const auto wall_now = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch());
  const auto steady_now = std::chrono::steady_clock::now();

  std::vector<Metric> metrics;
  metrics.reserve(zones_.size());

  for (auto& zone : zones_) {
    if (zone.energy_fd < 0) continue;

    const auto current_energy_uj = ReadEnergyUJ(zone.energy_fd);
    if (!current_energy_uj.has_value()) {
      continue;
    }

    uint64_t delta_energy_uj = 0;
    if (current_energy_uj.value() >= zone.last_energy_uj) {
      delta_energy_uj = current_energy_uj.value() - zone.last_energy_uj;
    } else if (zone.max_energy_range_uj > 0) {
      delta_energy_uj = current_energy_uj.value() + zone.max_energy_range_uj -
                        zone.last_energy_uj;
    } else {
      zone.last_energy_uj = current_energy_uj.value();
      zone.last_sample_time = steady_now;
      continue;
    }

    const auto delta_time_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            steady_now - zone.last_sample_time)
            .count();
    if (delta_time_ns <= 0) {
      zone.last_energy_uj = current_energy_uj.value();
      zone.last_sample_time = steady_now;
      continue;
    }

    Metric metric;
    metric.type = MetricType::METRIC_TYPE_CPU_POWER_PACKAGE;
    CpuID cpu_id;
    cpu_id.set_socket_index(zone.socket_index);
    cpu_id.set_core_index(0);
    metric.devId = DeviceId{std::move(cpu_id)};
    metric.value = static_cast<double>(delta_energy_uj) * 1000.0 /
                   static_cast<double>(delta_time_ns);
    metric.timestamp = wall_now.count();
    metrics.push_back(std::move(metric));

    zone.last_energy_uj = current_energy_uj.value();
    zone.last_sample_time = steady_now;
  }

  return metrics;
}

std::vector<MetricType> RaplCollector::Satisfiable() {
  return {MetricType::METRIC_TYPE_CPU_POWER_PACKAGE};
}

bool RaplCollector::OpenPowercap() {
  zones_.clear();

  for (const auto& zone : DiscoverZones()) {
    PowercapZone opened_zone;
    opened_zone.path = zone.path;
    opened_zone.socket_index = zone.socket_index;

    opened_zone.energy_fd =
        open((opened_zone.path / kEnergyFile).c_str(), O_RDONLY | O_CLOEXEC);
    if (opened_zone.energy_fd < 0) {
      continue;
    }

    const auto initial_energy = ReadEnergyUJ(opened_zone.energy_fd);
    if (!initial_energy.has_value()) {
      close(opened_zone.energy_fd);
      continue;
    }

    opened_zone.last_energy_uj = initial_energy.value();
    opened_zone.last_sample_time = std::chrono::steady_clock::now();

    uint64_t max_range_uj = 0;
    if (ReadUnsignedFile(opened_zone.path / kMaxEnergyRangeFile,
                         &max_range_uj)) {
      opened_zone.max_energy_range_uj = max_range_uj;
    }

    zones_.push_back(std::move(opened_zone));
  }

  return !zones_.empty();
}

void RaplCollector::ClosePowercap() {
  for (auto& zone : zones_) {
    if (zone.energy_fd >= 0) {
      close(zone.energy_fd);
      zone.energy_fd = -1;
    }
  }
  zones_.clear();
}

RaplCollector::~RaplCollector() { ClosePowercap(); }

}  // namespace collectors
}  // namespace agent
}  // namespace volta
