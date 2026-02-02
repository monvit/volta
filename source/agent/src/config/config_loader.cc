#include "config/config_loader.h"

#include <sched.h>

#include <chrono>
#include <cstdint>
#include <iostream>

#include "utils/utils.h"

namespace volta {
namespace agent {
namespace config {

std::filesystem::path ConfigLoader::kConfigFile = "agent.conf";

std::set<std::string_view, std::less<>> ConfigLoader::kValidTopLevelKeys = {
    "core_affinity", "interval", "server_address", "server_port", "collectors"};

std::map<std::string_view, std::set<std::string_view, std::less<>>, std::less<>>
    ConfigLoader::kValidCollectors = {
        {"cpu", {"proc_stat", "cpu_freq", "rapl", "zenpower", "pmu"}},
        {"gpu", {"nvml", "dcgm", "rocm", "level_zero"}},
        {"ram", {"mem_info", "vm_stat"}},
        {"io", {"disk_stats", "net_dev"}}};

Config ConfigLoader::LoadConfig() {
  Config config = LoadDefaultConfig();
  LoadConfigFile(config);
  return config;
}

Config ConfigLoader::LoadDefaultConfig() {
  Config config;

  // load config
  // if id file exitsts load else genereate uuid and save to file
  LoadUUID(config);

  CollectorConfig nvml_collector;
  nvml_collector.enabled = true;
  nvml_collector.metrics = {
      {"gpu_utilization", true},
      {"memory_utilization", true},
      {"temperature", true},
  };
  config.collectors[CollectorNames::kNvml] = nvml_collector;

  CollectorConfig proc_stat_config;
  proc_stat_config.enabled = true;
  proc_stat_config.metrics["cpu_usage_percent"] = true;
  config.collectors[CollectorNames::kProcStat] = proc_stat_config;

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
    LoadCollectors(tbl, out_config);
    CheckKeys(tbl);
  } catch (const toml::parse_error& err) {
    std::cerr << "Parsing Agent config failed: " << err.description() << " at "
              << err.source().begin << std::endl;
  }
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
    namespace utils = volta::agent::utils;
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

void ConfigLoader::LoadCollectors(toml::table& tbl, Config& out_config) {
  if (!tbl.contains("collectors")) return;

  auto collectors_node = tbl["collectors"].as_table();

  for (auto&& [hardware_type, hardware_node] : *collectors_node) {
    if (!kValidCollectors.contains(hardware_type.str())) {
      std::cerr << "Invalid hardware type: " << hardware_type << std::endl;
      continue;
    }

    auto collectors = hardware_node.as_array();
    if (!collectors) {
      std::cout << "Element " << hardware_type << " is not an array\n";
      continue;
    }

    CollectorConfig collector_config;

    std::cout << hardware_type << std::endl;
    for (auto&& collector : *collectors) {
      if (auto str = collector.value<std::string>()) {
        const auto& collector_set = kValidCollectors[hardware_type.str()];
        if (!collector_set.contains(*str)) {
          std::cout << "Invalid collector: " << *str
                    << ", for hardware: " << hardware_type << std::endl;
          continue;
        }

        std::cout << *str << std::endl;
        // TODO: Add metrics

        collector_config.enabled = !collector_config.metrics.empty();
      } else {
        std::cerr << "Invalid type in " << hardware_type << " array\n";
      }
    }
  }
}

void ConfigLoader::CheckKeys(toml::table& tbl) {
  for (auto&& [key, value] : tbl) {
    if (!kValidTopLevelKeys.contains(key.str())) {
      std::cout << "Key '" << key << "' is not a valid key\n";
    }
  }
}

void ConfigLoader::LoadUUID(Config& out_config) {}

}  // namespace config
}  // namespace agent
}  // namespace volta
