#include "rapl_collector.h"

#include <dirent.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>

namespace volta {
namespace agent {
namespace collectors {

namespace {

constexpr const char* kPowercapRoot = "/sys/class/powercap";

bool StartsWith(const std::string& s, const char* prefix) {
  const size_t n = std::strlen(prefix);
  return s.size() >= n && s.compare(0, n, prefix) == 0;
}

}  // namespace

bool RaplCollector::IsPackageName(const std::string& name) {
  if (name == "package") return true;
  if (!StartsWith(name, "package-")) return false;
  const char* p = name.c_str() + 8;
  if (*p == '\0') return false;
  while (*p) {
    if (!std::isdigit(static_cast<unsigned char>(*p))) return false;
    ++p;
  }
  return true;
}

std::optional<int> RaplCollector::ParseSocketIndex(const std::string& name) {
  if (name == "package") return 0;
  if (!StartsWith(name, "package-")) return std::nullopt;
  try {
    return std::stoi(name.substr(8));
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

bool RaplCollector::ReadU64File(const std::string& path, uint64_t* out) const {
  std::ifstream in(path);
  if (!in) return false;
  uint64_t v = 0;
  in >> v;
  if (!in && !in.eof()) return false;
  *out = v;
  return true;
}

bool RaplCollector::ReadNameFile(const std::string& path,
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
  DIR* d = opendir(kPowercapRoot);
  if (!d) return packages;

  while (dirent* ent = readdir(d)) {
    if (ent->d_name[0] == '.') continue;
    const std::string zone_dir = std::string(kPowercapRoot) + "/" + ent->d_name;
    const std::string name_path = zone_dir + "/name";
    const std::string energy_path = zone_dir + "/energy_uj";

    if (access(energy_path.c_str(), F_OK) != 0) continue;

    std::string name;
    if (!ReadNameFile(name_path, &name)) continue;
    if (!IsPackageName(name)) continue;

    PackageZone z;
    z.path = zone_dir;
    z.name = name;
    auto sock = ParseSocketIndex(name);
    z.socket_index = sock.value_or(-1);
    uint64_t max_range = 0;
    (void)ReadU64File(zone_dir + "/max_energy_range_uj", &max_range);
    z.max_energy_range_uj = max_range;
    packages.push_back(std::move(z));
  }
  closedir(d);

  std::sort(packages.begin(), packages.end(),
            [](const PackageZone& a, const PackageZone& b) {
              if (a.socket_index != b.socket_index)
                return a.socket_index < b.socket_index;
              return a.name < b.name;
            });
  return packages;
}

bool RaplCollector::IsSupported() {
  const auto packages = DiscoverPackages();
  for (const auto& p : packages) {
    if (access((p.path + "/energy_uj").c_str(), R_OK) == 0) return true;
  }
  return false;
}

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
  if (!ReadU64File(it->path + "/energy_uj", &energy)) return false;

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
  if (!ReadU64File(active_.path + "/energy_uj", &energy)) return {};

  const uint64_t delta_uj =
      DeltaEnergyUj(energy, last_energy_uj_, active_.max_energy_range_uj);
  last_energy_uj_ = energy;

  Metric m;
  m.type = MetricType::METRIC_TYPE_CPU_POWER_PACKAGE;
  CpuID cpu_id;
  cpu_id.set_socket_index(active_.socket_index >= 0 ? active_.socket_index : 0);
  cpu_id.set_core_index(0);
  m.devId = DeviceId{std::move(cpu_id)};
  m.value = static_cast<double>(delta_uj) / 1.0e6;
  m.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
  return {m};
}

std::vector<MetricType> RaplCollector::Satisfiable() {
  return {MetricType::METRIC_TYPE_CPU_POWER_PACKAGE};
}

}  // namespace collectors
}  // namespace agent
}  // namespace volta
