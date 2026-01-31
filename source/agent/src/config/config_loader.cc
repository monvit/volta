#include "config/config_loader.h"

#include <sched.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <toml++/toml.hpp>
#include <unordered_map>

#include "config/config.h"

namespace volta {
namespace agent {
namespace config {

#if defined(DEBUG)
std::filesystem::path ConfigLoader::kConfigFile = "etc/volta/agent.conf";
#elif defined(RELEASE)
std::filesystem::path ConfigLoader::kConfigFile = "/etc/volta/agent.conf";
#else
std::filesystem::path ConfigLoader::kConfigFile = "agent.conf";
#endif

std::set<std::string> ConfigLoader::kValidTopLevelKeys = {
    "core_affinity",  "core_affinity_mask", "interval",
    "server_address", "server_port",        "collectors"};

std::map<std::string, std::set<std::string>>
    ConfigLoader::kValidCollectorMetrics = {
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
  if (!std::filesystem::exists(kConfigFile)) {
    std::cout << "Agent config file not found, loading default settings."
              << std::endl;
    return;
  }

  try {
    toml::table tbl = toml::parse_file(kConfigFile.string());

    if (auto val = tbl["core_affinity"]) {
      unsigned int max_cpu = MaxOnlineCpus();
      cpu_set_t mask;
      CPU_ZERO(&mask);

      // core_affinity = "all"
      if (auto s = val.value<std::string>(); s && *s == "all") {
        for (unsigned int i = 0; i < max_cpu; ++i) CPU_SET(i, &mask);

        out_config.core_affinity = mask;
      }
      // core_affinity = [ ... ]
      else if (auto arr = val.as_array()) {
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
    // interval
    // server_address
    // server_port

    auto collectors_node = tbl["collectors"].as_table();
    if (!collectors_node) return;

    for (auto& [collector_name, collector_node] : *collectors_node) {
      auto collector_table = collector_node.as_table();
      if (!collector_table) continue;

      CollectorConfig collector;

      if (auto enabled_array = (*collector_table)["enabled"].as_array()) {
        for (auto& item : *enabled_array) {
          if (auto str = item.value<std::string>()) {
            collector.metrics[*str] = true;
          }
        }

        collector.enabled = !collector.metrics.empty();
      }

      out_config.collectors[std::string{collector_name.str()}] = collector;
    }
  } catch (const toml::parse_error& err) {
    std::cerr << "Parsing Agent config failed: " << err.description() << " at "
              << err.source().begin << std::endl;
  }
}

}  // namespace config
}  // namespace agent
}  // namespace volta
