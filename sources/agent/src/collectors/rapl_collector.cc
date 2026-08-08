#include "rapl_collector.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <regex>

namespace volta {
namespace agent {
namespace collectors {

namespace {

constexpr const char* kPowercapRoot = "/sys/class/powercap";
constexpr double kMicrojoulesPerJoule = 1.0e6;

const std::regex kPackageNameRegex(R"(^package(-[0-9]+)?$)");
const std::regex kPackageIndexRegex(R"(^package-([0-9]+)$)");

}  // namespace

bool RaplCollector::IsPackageName(const std::string& name) {
  return std::regex_match(name, kPackageNameRegex);
}

std::optional<int> RaplCollector::ParseSocketIndex(const std::string& name) {
  if (name == "package") return 0;
  std::smatch match;
  if (!std::regex_match(name, match, kPackageIndexRegex)) {
    return std::nullopt;
  }
  try {
    return std::stoi(match[1].str());
  } catch (...) {
    return std::nullopt;
  }
}

uint64_t RaplCollector::DeltaEnergyUj(uint64_t now, uint64_t last,
                                      uint64_t max_range_uj) {
  if (now >= last) return now - last;
  if (max_range_uj > 0 && last <= max_range_uj) {
    return (max_range_uj - last) + now;
  }
  return (std::numeric_limits<uint64_t>::max() - last) + now + 1;
}

bool RaplCollector::ReadU64File(const std::filesystem::path& path,
                                uint64_t* out) const {
  std::ifstream in(path);
  if (!in) return false;
  uint64_t v = 0;
  in >> v;
  if (!in && !in.eof()) return false;
  *out = v;
  return true;
}

bool RaplCollector::ReadNameFile(const std::filesystem::path& path,
                                 std::string* out) const {
  std::ifstream in(path);
  if (!in) return false;
  std::string line;
  if (!std::getline(in, line)) return false;
  while (!line.empty() &&
         (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
    line.pop_back();
  }
  *out = line;
  return true;
}

std::vector<RaplCollector::PackageZone> RaplCollector::DiscoverPackages()
    const {
  std::vector<PackageZone> packages;
  std::error_code ec;
  const std::filesystem::path root(kPowercapRoot);
  if (!std::filesystem::is_directory(root, ec)) return packages;

  for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
    if (!entry.is_directory()) continue;

    const auto zone_dir = entry.path();
    const auto energy_path = zone_dir / "energy_uj";
    std::ifstream energy_in(energy_path);
    if (!energy_in) continue;

    std::string name;
    if (!ReadNameFile(zone_dir / "name", &name)) continue;
    if (!IsPackageName(name)) continue;

    PackageZone z;
    z.path = zone_dir;
    z.name = name;
    auto sock = ParseSocketIndex(name);
    z.socket_index = sock.value_or(-1);
    uint64_t max_range = 0;
    (void)ReadU64File(zone_dir / "max_energy_range_uj", &max_range);
    z.max_energy_range_uj = max_range;
    packages.push_back(std::move(z));
  }

  std::sort(packages.begin(), packages.end(),
            [](const PackageZone& a, const PackageZone& b) {
              if (a.socket_index != b.socket_index)
                return a.socket_index < b.socket_index;
              return a.name < b.name;
            });
  return packages;
}

bool RaplCollector::IsSupported() { return !DiscoverPackages().empty(); }

bool RaplCollector::Init() {
  initialized_ = false;
  auto packages = DiscoverPackages();
  if (packages.empty()) return false;

  auto it =
      std::find_if(packages.begin(), packages.end(),
                   [](const PackageZone& z) { return z.socket_index == 0; });
  if (it == packages.end()) {
    it = packages.begin();
    it->socket_index = 0;
  }

  uint64_t energy = 0;
  if (!ReadU64File(it->path / "energy_uj", &energy)) return false;

  active_ = *it;
  last_energy_uj_ = energy;
  initialized_ = true;
  return true;
}

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

  uint64_t energy = 0;
  if (!ReadU64File(active_.path / "energy_uj", &energy)) return {};

  const uint64_t delta_uj =
      DeltaEnergyUj(energy, last_energy_uj_, active_.max_energy_range_uj);
  last_energy_uj_ = energy;

  Metric m;
  m.type = MetricType::METRIC_TYPE_CPU_POWER_PACKAGE;
  CpuID cpu_id;
  cpu_id.set_socket_index(active_.socket_index >= 0 ? active_.socket_index : 0);
  cpu_id.set_core_index(0);
  m.devId = DeviceId{std::move(cpu_id)};
  m.value = static_cast<double>(delta_uj) / kMicrojoulesPerJoule;
  m.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
  return {m};
}

std::vector<MetricType> RaplCollector::Satisfiable() {
  return {MetricType::METRIC_TYPE_CPU_POWER_PACKAGE};
}

}  // namespace collectors
}  // namespace agent
}  // namespace volta
