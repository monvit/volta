#include "config/config_loader.h"

#include <sched.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "utils/utils.h"

namespace volta {
namespace agent {
namespace config {

// TODO: different path for prod build
std::filesystem::path ConfigLoader::kConfigFile = "agent.conf";
std::filesystem::path ConfigLoader::kUUIDFile = "agent.uuid";

std::set<std::string_view, std::less<>> ConfigLoader::kValidTopLevelKeys = {
    "core_affinity", "interval", "server_address", "server_port", "metrics"};

Config ConfigLoader::LoadConfig() {
  Config config = LoadDefaultConfig();
  LoadConfigFile(config);
  return config;
}

Config ConfigLoader::LoadDefaultConfig() {
  Config config;

  if (!LoadUUID(config)) CreateUUID(config);

  // request all metrics by default for now
  config.requestedMetrics = {
      MetricType::METRIC_TYPE_UNSPECIFIED,
      MetricType::METRIC_TYPE_CPU_POWER_PACKAGE,
      MetricType::METRIC_TYPE_CPU_POWER_CORES,
      MetricType::METRIC_TYPE_CPU_CLOCK_SPEED,
      MetricType::METRIC_TYPE_CPU_UTILIZATION,
      MetricType::METRIC_TYPE_CPU_TEMPERATURE,
      MetricType::METRIC_TYPE_CPU_IOWAIT,
      MetricType::METRIC_TYPE_CPU_CACHE_HIT_RATIO,
      MetricType::METRIC_TYPE_CPU_ACTIVE_PROCESSES,
      MetricType::METRIC_TYPE_GPU_POWER,
      MetricType::METRIC_TYPE_GPU_CLOCK_SPEED,
      MetricType::METRIC_TYPE_GPU_UTILIZATION,
      MetricType::METRIC_TYPE_GPU_TEMPERATURE,
      MetricType::METRIC_TYPE_GPU_VRAM_USED,
      MetricType::METRIC_TYPE_GPU_PCIE_BANDWIDTH,
      MetricType::METRIC_TYPE_GPU_COMPUTE_UNIT_UTILIZATION,
      MetricType::METRIC_TYPE_GPU_SHARED_MEMORY_UTILIZATION,
      MetricType::METRIC_TYPE_GPU_REGISTER_UTILIZATION,
      MetricType::METRIC_TYPE_RAM_POWER,
      MetricType::METRIC_TYPE_RAM_TOTAL,
      MetricType::METRIC_TYPE_RAM_AVAILABLE,
      MetricType::METRIC_TYPE_RAM_USED,
      MetricType::METRIC_TYPE_RAM_CACHED,
      MetricType::METRIC_TYPE_SWAP_USED,
      MetricType::METRIC_TYPE_SWAP_ACTIVITY,
      MetricType::METRIC_TYPE_DISK_READ_THROUGHPUT,
      MetricType::METRIC_TYPE_DISK_WRITE_THROUGHPUT,
      MetricType::METRIC_TYPE_DISK_READ_IOPS,
      MetricType::METRIC_TYPE_DISK_WRITE_IOPS,
      MetricType::METRIC_TYPE_DISK_BUSY_TIME,
      MetricType::METRIC_TYPE_DISK_CAPACITY_USED,
      MetricType::METRIC_TYPE_NET_BYTES_RECEIVED,
      MetricType::METRIC_TYPE_NET_BYTES_SENT,
      MetricType::METRIC_TYPE_NET_PACKETS_RECEIVED,
      MetricType::METRIC_TYPE_NET_PACKETS_SENT,
  };

  return config;
}

inline unsigned int MaxOnlineCpus() {
  long n = sysconf(_SC_NPROCESSORS_ONLN);
  return (n > 0) ? static_cast<unsigned int>(n) : 0;
}

bool AddCpu(cpu_set_t& set, unsigned int cpu, unsigned int max_cpu) {
  if (cpu >= max_cpu) return false;
  CPU_SET(cpu, &set);
  return true;
}

bool AddRange(cpu_set_t& set, unsigned int from, unsigned int to,
              unsigned int max_cpu) {
  if (from > to || to >= max_cpu) return false;
  for (unsigned int i = from; i <= to; ++i) CPU_SET(i, &set);
  return true;
}

void ConfigLoader::LoadConfigFile(Config& out_config) {
  // TODO: Proper logging
  if (!std::filesystem::exists(kConfigFile)) {
    std::cout << "Agent config file not found, loading default settings."
              << std::endl;
    return;
  }

  try {
    toml::table tbl = toml::parse_file(kConfigFile.string());

    LoadCoreAffinity(tbl, out_config);
    LoadInterval(tbl, out_config);
    LoadServerAddress(tbl, out_config);
    LoadServerPort(tbl, out_config);
    LoadMetrics(tbl, out_config);
    CheckKeys(tbl);
  } catch (const toml::parse_error& err) {
    std::cerr << "Parsing Agent config failed: " << err.description() << " at "
              << err.source().begin << std::endl;
  }
}

bool ConfigLoader::LoadUUID(Config& out_config) {
  if (!std::filesystem::exists(kUUIDFile)) return false;

  // TODO: Handle errors
  std::fstream f(kUUIDFile);
  std::string uuid;
  std::getline(f, uuid);
  out_config.uuid = uuid;
  return true;
}

void ConfigLoader::CreateUUID(Config& out_config) {
  // TODO: Handle errors
  std::string uuid = utils::GenerateUUIDv4();

  std::filesystem::path tmp = kUUIDFile;
  tmp += ".tmp";

  {
    std::ofstream f(tmp, std::ios::trunc);
    f << uuid;
    f.flush();
  }

  std::filesystem::rename(tmp, kUUIDFile);
  out_config.uuid = uuid;
}

void ConfigLoader::LoadCoreAffinity(toml::table& tbl, Config& out_config) {
  if (!tbl.contains("core_affinity")) return;

  auto val = tbl["core_affinity"];

  // core_affinity = "all"
  if (auto s = val.value<std::string>(); s && *s == "all") {
    out_config.core_affinity = Config::kDefaultAffinity;
  }
  // core_affinity = [ ... ]
  else if (auto arr = val.as_array()) {
    unsigned int max_cpu = MaxOnlineCpus();
    cpu_set_t mask;
    CPU_ZERO(&mask);
    // NOTE: Should setting the affinity in here?
    for (auto& item : *arr) {
      // liczba CPU
      if (auto cpu = item.value<unsigned int>()) {
        if (!AddCpu(mask, *cpu, max_cpu)) {
          std::cerr << "CPU index out of range: " << *cpu << "\n";
          return;
        }
      }
      // zakres "X-Y"
      else if (auto str = item.value<std::string>()) {
        unsigned int from, to;
        if (sscanf(str->c_str(), "%u-%u", &from, &to) == 2) {
          if (!AddRange(mask, from, to, max_cpu)) {
            std::cerr << "Invalid CPU range: " << *str << "\n";
            return;
          }
        } else {
          std::cerr << "Invalid core_affinity entry: " << *str << "\n";
          return;
        }
      } else {
        std::cerr << "Invalid core_affinity element type\n";
        return;
      }
    }
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
      perror("sched_setaffinity");
    } else {
      std::cout << "Successfully set CPU affinity mask." << std::endl;
    }

    out_config.core_affinity = mask;
  } else {
    std::cerr << "Invalid core_affinity value\n";
  }
}

void ConfigLoader::LoadInterval(toml::table& tbl, Config& out_config) {
  if (!tbl.contains("interval")) return;

  auto val = tbl["interval"];

  if (auto ms = val.value<uint32_t>()) {
    out_config.collection_interval = std::chrono::milliseconds(*ms);
    std::cout << "Collection Interval set to: "
              << out_config.collection_interval << std::endl;
  } else {
    std::cerr << "Invalid interval value type, use uint32" << std::endl;
  }
}

void ConfigLoader::LoadServerAddress(toml::table& tbl, Config& out_config) {
  if (!tbl.contains("server_address")) return;

  auto val = tbl["server_address"];

  if (auto str = val.value<std::string>()) {
    if (utils::IsValidIP(*str) || utils::IsResolvable(*str)) {
      out_config.server_address = *str;
      std::cout << "Server Address set to " << *str << std::endl;
    } else {
      std::cerr << "Invalid server_address format" << std::endl;
    }
  } else {
    std::cerr << "Invalid server_address value type, use string" << std::endl;
  }
}

void ConfigLoader::LoadServerPort(toml::table& tbl, Config& out_config) {
  if (!tbl.contains("server_port")) return;

  auto val = tbl["server_port"];

  if (auto port = val.value<uint16_t>(); port && *port > 0) {
    out_config.server_port = *port;
    std::cout << "Server port set to " << *port << std::endl;
  } else {
    std::cerr << "server_port has an incorrect type or value, use number "
                 "from range "
                 "[1, 65535]"
              << std::endl;
  }
}

void ConfigLoader::LoadMetrics(toml::table& tbl, Config& out_config) {
  if (!tbl.contains("metrics")) return;

  auto metrics_node = tbl["metrics"];

  std::vector<MetricType> metrics;

  auto append_metric = [&](MetricType metric) {
    if (metric == MetricType::METRIC_TYPE_UNSPECIFIED) {
      return;
    }
    if (std::find(metrics.begin(), metrics.end(), metric) == metrics.end()) {
      metrics.push_back(metric);
    }
  };

  auto parse_metric_name = [&](const std::string& name,
                               MetricType& metric) -> bool {
    if (MetricType_Parse(name, &metric)) return true;
    if (MetricType_Parse(std::string("METRIC_TYPE_") + name, &metric))
      return true;
    return false;
  };

  if (auto arr = metrics_node.as_array()) {
    for (const auto& item : *arr) {
      if (auto metric_num = item.value<int>()) {
        if (!MetricType_IsValid(*metric_num)) {
          std::cerr << "Invalid metric value: " << *metric_num << std::endl;
          continue;
        }
        append_metric(static_cast<MetricType>(*metric_num));
        continue;
      }

      if (auto metric_name = item.value<std::string>()) {
        MetricType metric;
        if (!parse_metric_name(*metric_name, metric)) {
          std::cerr << "Invalid metric name: " << *metric_name << std::endl;
          continue;
        }
        append_metric(metric);
        continue;
      }

      std::cerr << "Invalid metric entry type" << std::endl;
    }
  } else if (auto val = metrics_node.value<std::string>()) {
    MetricType metric;
    if (parse_metric_name(*val, metric)) {
      append_metric(metric);
    } else {
      std::cerr << "Invalid metric name: " << *val << std::endl;
    }
  } else {
    std::cerr << "Invalid metrics value, use array of strings or numbers"
              << std::endl;
    return;
  }

  if (!metrics.empty()) {
    out_config.requestedMetrics = std::move(metrics);
    std::cout << "Requested metrics updated to "
              << out_config.requestedMetrics.size() << " entries" << std::endl;
  }
}

void ConfigLoader::CheckKeys(toml::table& tbl) {
  for (auto&& [key, value] : tbl) {
    if (!kValidTopLevelKeys.contains(key.str())) {
      std::cout << "Key '" << key << "' is not a valid key\n";
    }
  }
}

}  // namespace config
}  // namespace agent
}  // namespace volta
