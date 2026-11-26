#include "exporter.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <format>
#include <sstream>
#include <system_error>
#include <utility>
#include <variant>

namespace volta {
namespace agent {
namespace {

DumpRequestResult OpenFailure(int err, const std::filesystem::path& dir,
                              const std::filesystem::path& file) {
  if (err == ENOENT) {
    return {DumpRequestStatus::kDirectoryMissing, dir, 0};
  }
  if (err == ENOTDIR) {
    return {DumpRequestStatus::kNotADirectory, dir, 0};
  }
  return {DumpRequestStatus::kOpenFailed, file, err != 0 ? err : EIO};
}

}  // namespace

Exporter::Exporter(const config::Config& cfg) : dump_dir_(cfg.dump_dir) {}

void Exporter::Dump(const std::vector<Metric>& metrics) {
  std::lock_guard lock(mu_);
  if (!dump_file_.is_open()) return;

  for (const auto& metric : metrics)
    dump_file_ << MetricType_Name(metric.type) << ";" << DescribeDevice(metric)
               << ";" << metric.timestamp << ";" << metric.value << "\n";
  dump_file_.flush();
}

DumpRequestResult Exporter::StartDump(
    std::optional<std::filesystem::path> dump_dir_overload) {
  std::lock_guard lock(mu_);
  if (dump_file_.is_open()) {
    return {DumpRequestStatus::kAlreadyRunning, active_path_, 0};
  }

  const auto dir =
      dump_dir_overload.has_value() ? *dump_dir_overload : dump_dir_;
  if (dir.empty()) {
    return {DumpRequestStatus::kDirectoryMissing, dir, 0};
  }

  const auto file_path =
      dir / std::format("dump_{}.csv", std::chrono::system_clock::now());

  errno = 0;
  dump_file_.open(file_path, std::ios::out | std::ios::trunc);
  const int open_errno = errno;
  if (!dump_file_.is_open()) {
    return OpenFailure(open_errno, dir, file_path);
  }

  errno = 0;
  dump_file_ << "metric_type;device_id;timestamp;value\n";
  dump_file_.flush();
  const int write_errno = errno;
  if (!dump_file_) {
    // close() retries the flush and remove() both overwrite errno.
    const int err = write_errno != 0 ? write_errno : EIO;
    dump_file_.close();
    std::error_code ec;
    std::filesystem::remove(file_path, ec);
    return {DumpRequestStatus::kOpenFailed, file_path, err};
  }

  active_path_ = file_path;
  return {DumpRequestStatus::kStarted, active_path_, 0};
}

std::optional<std::filesystem::path> Exporter::EndDump() {
  std::lock_guard lock(mu_);
  if (!dump_file_.is_open()) return std::nullopt;

  dump_file_.close();
  return std::exchange(active_path_, {});
}

bool Exporter::IsActive() const {
  std::lock_guard lock(mu_);
  return dump_file_.is_open();
}

std::string Exporter::DescribeDevice(const Metric& m) {
  std::ostringstream out;
  if (m.devId.has_value())
    std::visit(
        [&](const auto& id) {
          using T = std::decay_t<decltype(id)>;
          if constexpr (std::is_same_v<T, GpuID>) {
            out << "gpu=" << id.pci_domain() << ':'
                << static_cast<int>(id.pci_bus()) << ':'
                << static_cast<int>(id.pci_device()) << '.'
                << static_cast<int>(id.pci_function());
          } else if constexpr (std::is_same_v<T, CpuID>) {
            out << "cpu=" << static_cast<int>(id.socket_index()) << "/"
                << id.core_index();
          } else if constexpr (std::is_same_v<T, NetInterfaceID>) {
            out << "net_ifindex=" << id.ifindex();
          } else if constexpr (std::is_same_v<T, DiskID>) {
            out << "disk=" << id.major() << ':' << id.minor();
          }
        },
        *m.devId);
  else
    out << "system";
  return out.str();
}

control::ControlReply FormatDumpReply(const DumpRequestResult& result) {
  switch (result.status) {
    case DumpRequestStatus::kStarted:
      return {true, "Dump started: " + result.path.string()};
    case DumpRequestStatus::kAlreadyRunning:
      return {false, "Dump already running: " + result.path.string()};
    case DumpRequestStatus::kDirectoryMissing:
      if (result.path.empty()) {
        return {false, "Dump directory not configured"};
      }
      return {false, "Dump directory does not exist: " + result.path.string()};
    case DumpRequestStatus::kNotADirectory:
      return {false, "Dump path is not a directory: " + result.path.string()};
    case DumpRequestStatus::kOpenFailed:
      return {false, "Cannot open dump file: " + result.path.string() + ": " +
                         std::strerror(result.error)};
  }
  return {false, "Internal error"};
}

control::ControlReply FormatDumpReply(
    const std::optional<std::filesystem::path>& stopped) {
  if (!stopped.has_value()) return {true, "No dump was running"};
  return {true, "Dump stopped: " + stopped->string()};
}

}  // namespace agent
}  // namespace volta
