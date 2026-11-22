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
#include <sstream>
#include <string>

#include "collectors/collector.h"
#include "config_loader.h"
#include "logger/spdlog_logger.h"
#include "utils/utils.h"

namespace volta {
namespace agent {
namespace config {
using logging::LogLevel;

std::set<std::string_view, std::less<>> ConfigLoader::kValidTopLevelKeys = {
    "core_affinity", "interval",    "server_address", "server_port",
    "metrics",       "time_window", "log_level"};

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

  // Derive defaults from the metrics implemented by registered collectors so
  // newly added or unsupported metric types cannot drift into this list.
  config.requestedMetrics =
      collectors::CollectorRegistry::Instance().SatisfiableMetrics();

  return config;
}

std::string ConfigLoader::SerializeConfig(const Config& config) {
  toml::table table;

  bool default_affinity = true;
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
    if (CPU_ISSET(cpu, &config.core_affinity) !=
        CPU_ISSET(cpu, &Config::kDefaultAffinity)) {
      default_affinity = false;
      break;
    }
  }
  if (default_affinity) {
    table.insert("core_affinity", "all");
  } else {
    toml::array affinity;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
      if (CPU_ISSET(cpu, &config.core_affinity)) affinity.push_back(cpu);
    }
    table.insert("core_affinity", std::move(affinity));
  }

  table.insert("interval",
               static_cast<int64_t>(config.collection_interval.count()));
  table.insert("server_address", config.server_address);
  table.insert("server_port", static_cast<int64_t>(config.server_port));

  toml::array metrics;
  for (const auto metric : config.requestedMetrics) {
    metrics.push_back(MetricType_Name(metric));
  }
  table.insert("metrics", std::move(metrics));
  table.insert(
      "time_window",
      static_cast<double>(config.buffered_time_window.count()) / 1000.0);

  switch (config.log_level) {
    case LogLevel::NOISY_LEVEL:
      table.insert("log_level", "trace");
      break;
    case LogLevel::DEBUG_LEVEL:
      table.insert("log_level", "debug");
      break;
    case LogLevel::INFO_LEVEL:
      table.insert("log_level", "info");
      break;
    case LogLevel::WARNING_LEVEL:
      table.insert("log_level", "warning");
      break;
    case LogLevel::ERROR_LEVEL:
      table.insert("log_level", "error");
      break;
  }

  std::ostringstream serialized;
  serialized << table;
  return serialized.str();
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
  try {
    toml::table tbl = toml::parse_file(path.string());

    LoadLogging(tbl, out_config);
    logger_->SetLevel(out_config.log_level);

    LoadCoreAffinity(tbl, out_config);
    LoadInterval(tbl, out_config);
    LoadTimeWindow(tbl, out_config);
    LoadServerAddress(tbl, out_config);
    LoadServerPort(tbl, out_config);
    LoadMetrics(tbl, out_config);
    CheckKeys(tbl);
  } catch (const toml::parse_error& err) {
    logger_->Log(LogLevel::ERROR_LEVEL,
                 "Parsing Agent config failed: {} at {} ({})",
                 err.description(), err.source().begin.line, path.string());
    throw;
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
}

void ConfigLoader::CheckKeys(toml::table& tbl) {
  for (auto&& [key, value] : tbl) {
    if (!kValidTopLevelKeys.contains(key.str())) {
      logger_->Log(LogLevel::WARNING_LEVEL, "Key '{}' is not a valid key",
                   key.str());
    }
  }
}

}  // namespace config
}  // namespace agent
}  // namespace volta
