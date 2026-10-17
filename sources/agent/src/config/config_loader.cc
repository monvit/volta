#include "config/config_loader.h"

#include <sched.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "config_loader.h"
#include "logger/spdlog_logger.h"
#include "utils/utils.h"

namespace volta {
namespace agent {
namespace config {
using logging::LogLevel;

std::set<std::string_view, std::less<>> ConfigLoader::kValidTopLevelKeys = {
    "core_affinity", "interval",    "server_address", "server_port",
    "metrics",       "time_window", "log_level",      "log_file"};

// What logger should be used before config is parsed?
// stderr logger seems like a sane fallback
std::unique_ptr<logging::Logger> ConfigLoader::logger_ =
    std::make_unique<logging::SpdlogLogger>(
        spdlog::stderr_color_mt("Config Loader"));

Config ConfigLoader::LoadConfig(
    const std::optional<std::filesystem::path>& file) {
  Config config = LoadDefaultConfig();
  if (file) {
    LoadConfigFile(*file, config);
  }
  return config;
}

Config ConfigLoader::LoadDefaultConfig() {
  Config config;

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

void ConfigLoader::LoadConfigFile(const std::filesystem::path& path,
                                  Config& out_config) {
  if (!std::filesystem::exists(path)) {
    logger_->Log(LogLevel::ERROR_LEVEL,
                 "Agent config file not found, loading default settings.");
    return;
  }
  toml::table tbl = toml::parse_file(path.string());

  try {
    LoadCoreAffinity(tbl, out_config);
    LoadInterval(tbl, out_config);
    LoadTimeWindow(tbl, out_config);
    LoadServerAddress(tbl, out_config);
    LoadServerPort(tbl, out_config);
    LoadMetrics(tbl, out_config);
    LoadLogging(tbl, out_config);
    CheckKeys(tbl);
  } catch (const toml::parse_error& err) {
    logger_->Log(LogLevel::ERROR_LEVEL, "Parsing Agent config failed: {} at {}",
                 err.description(), err.source().begin.line);
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
          logger_->Log(LogLevel::ERROR_LEVEL, "CPU index out of range: {}",
                       *cpu);
          return;
        }
      }
      // zakres "X-Y"
      else if (auto str = item.value<std::string>()) {
        unsigned int from, to;
        if (sscanf(str->c_str(), "%u-%u", &from, &to) == 2) {
          if (!AddRange(mask, from, to, max_cpu)) {
            logger_->Log(LogLevel::ERROR_LEVEL, "Invalid CPU range: {}", *str);
            return;
          }
        } else {
          logger_->Log(LogLevel::ERROR_LEVEL, "Invalid core_affinity entry: {}",
                       *str);
          return;
        }
      } else {
        logger_->Log(LogLevel::ERROR_LEVEL,
                     "Invalid core_affinity element type");
        return;
      }
    }
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
      logger_->Log(LogLevel::ERROR_LEVEL, "sched_setaffinity failed: {}",
                   std::strerror(errno));
    } else {
      logger_->Log(LogLevel::INFO_LEVEL, "Successfully set CPU affinity mask.");
    }

    out_config.core_affinity = mask;
  } else {
    logger_->Log(LogLevel::ERROR_LEVEL, "Invalid core_affinity value");
  }
}

void ConfigLoader::LoadInterval(toml::table& tbl, Config& out_config) {
  if (!tbl.contains("interval")) return;

  auto val = tbl["interval"];

  if (auto ms = val.value<uint32_t>()) {
    out_config.collection_interval = std::chrono::milliseconds(*ms);
    logger_->Log(LogLevel::INFO_LEVEL, "Collection Interval set to: {}",
                 out_config.collection_interval);
  } else {
    logger_->Log(LogLevel::ERROR_LEVEL,
                 "Invalid interval value type, use uint32");
  }
}

void ConfigLoader::LoadServerAddress(toml::table& tbl, Config& out_config) {
  if (!tbl.contains("server_address")) return;

  auto val = tbl["server_address"];

  if (auto str = val.value<std::string>()) {
    if (utils::IsValidIP(*str) || utils::IsResolvable(*str)) {
      out_config.server_address = *str;
      logger_->Log(LogLevel::INFO_LEVEL, "Server Address set to {}", *str);
    } else {
      logger_->Log(LogLevel::ERROR_LEVEL, "Invalid server_address format");
    }
  } else {
    logger_->Log(LogLevel::ERROR_LEVEL,
                 "Invalid server_address value type, use string");
  }
}

void ConfigLoader::LoadServerPort(toml::table& tbl, Config& out_config) {
  if (!tbl.contains("server_port")) return;

  auto val = tbl["server_port"];

  if (auto port = val.value<uint16_t>(); port && *port > 0) {
    out_config.server_port = *port;
    logger_->Log(LogLevel::INFO_LEVEL, "Server port set to {}", *port);
  } else {
    logger_->Log(LogLevel::ERROR_LEVEL,
                 "server_port has an incorrect type or value, use number  from "
                 "range [1, 65535]");
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
          logger_->Log(LogLevel::ERROR_LEVEL, "Invalid metric value: {}",
                       *metric_num);
          continue;
        }
        append_metric(static_cast<MetricType>(*metric_num));
        continue;
      }

      if (auto metric_name = item.value<std::string>()) {
        MetricType metric;
        if (!parse_metric_name(*metric_name, metric)) {
          logger_->Log(LogLevel::ERROR_LEVEL, "Invalid metric name: {}",
                       *metric_name);
          continue;
        }
        append_metric(metric);
        continue;
      }

      logger_->Log(LogLevel::ERROR_LEVEL, "Invalid metric entry type");
    }
  } else if (auto val = metrics_node.value<std::string>()) {
    MetricType metric;
    if (parse_metric_name(*val, metric)) {
      append_metric(metric);
    } else {
      logger_->Log(LogLevel::ERROR_LEVEL, "Invalid metric name: {}", *val);
    }
  } else {
    logger_->Log(LogLevel::ERROR_LEVEL,
                 "Invalid metrics value, use array of strings or numbers");
    return;
  }

  if (!metrics.empty()) {
    out_config.requestedMetrics = std::move(metrics);
    logger_->Log(LogLevel::INFO_LEVEL,
                 "Requested metrics updated to {} entries",
                 out_config.requestedMetrics.size());
  }
}

void ConfigLoader::LoadTimeWindow(toml::table& tbl, Config& out_config) {
  if (!tbl.contains("time_window")) {
    return;
  }

  auto val = tbl["time_window"];

  if (auto window = val.value<float>();
      window &&
      (*window * 1000) > (float)out_config.collection_interval.count()) {
    out_config.buffered_time_window =
        std::chrono::milliseconds((uint32_t)(*window * 1000));
    logger_->Log(LogLevel::INFO_LEVEL, "Time window set to {}", *window);
  } else {
    logger_->Log(LogLevel::ERROR_LEVEL,
                 "time_window has an incorrect type or value, use number "
                 "larger than collection interval ({}s)",
                 (float_t)out_config.collection_interval.count() / 1000.0);
  }
}

void ConfigLoader::LoadLogging(toml::table& tbl, Config& out_config) {
  if (auto level = tbl["log_level"].value<std::string>()) {
    std::string normalized = *level;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (normalized == "trace" || normalized == "noisy") {
      out_config.log_level = logging::LogLevel::NOISY_LEVEL;
    } else if (normalized == "debug") {
      out_config.log_level = logging::LogLevel::DEBUG_LEVEL;
    } else if (normalized == "info") {
      out_config.log_level = logging::LogLevel::INFO_LEVEL;
    } else if (normalized == "warn" || normalized == "warning") {
      out_config.log_level = logging::LogLevel::WARNING_LEVEL;
    } else if (normalized == "error") {
      out_config.log_level = logging::LogLevel::ERROR_LEVEL;
    } else {
      logger_->Log(LogLevel::ERROR_LEVEL,
                   "Invalid log_level '{}'; using default level", *level);
    }
  } else if (tbl.contains("log_level")) {
    logger_->Log(LogLevel::ERROR_LEVEL,
                 "Invalid log_level type; expected a string");
  }

  if (auto path = tbl["log_file"].value<std::string>()) {
    if (path->empty()) {
      out_config.log_file.reset();
    } else {
      out_config.log_file = std::filesystem::path(*path);
    }
  } else if (tbl.contains("log_file")) {
    logger_->Log(LogLevel::ERROR_LEVEL,
                 "Invalid log_file type; expected a string");
  }
}

void ConfigLoader::CheckKeys(toml::table& tbl) {
  for (auto&& [key, value] : tbl) {
    if (!kValidTopLevelKeys.contains(key.str())) {
      logger_->Log(LogLevel::INFO_LEVEL, "Key '{}' is not a valid key",
                   key.str());
    }
  }
}

}  // namespace config
}  // namespace agent
}  // namespace volta
