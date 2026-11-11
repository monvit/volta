#include "exporter.h"

#include <mutex>
#include <variant>

namespace volta {
namespace agent {

Exporter::Exporter(const config::Config& cfg) : dump_file_() {
  if (std::filesystem::exists(cfg.dump_dir)) {
    dump_dir_ = cfg.dump_dir;
  }
}

void Exporter::Dump(const std::vector<Metric>& metrics) {
  std::lock_guard lock(mu_);
  if (!is_exporting) return;

  for (auto metric : metrics)
    dump_file_ << MetricType_Name(metric.type) << ";" << DescribeDevice(metric)
               << ";" << metric.timestamp << ";" << metric.value << "\n";
  dump_file_.flush();
}

void Exporter::StartDump(
    std::optional<std::filesystem::path> dump_dir_overload) {
  std::lock_guard lock(mu_);
  if (is_exporting) return;

  auto dir = dump_dir_overload.has_value() ? *dump_dir_overload : dump_dir_;
  if (!std::filesystem::exists(dir)) return;

  dump_file_.open(dir.string() + std::format("/dump_{}.csv",
                                             std::chrono::system_clock::now()),
                  std::ios_base::out | std::ios_base::trunc);
  if (!dump_file_.is_open()) return;

  dump_file_ << "metric_type;device_id;timestamp;value\n";
  dump_file_.flush();
  is_exporting = true;
}

void Exporter::EndDump() {
  std::lock_guard lock(mu_);
  if (dump_file_.is_open()) dump_file_.close();
  is_exporting = false;
}

bool Exporter::IsActive() const {
  std::lock_guard lock(mu_);
  return is_exporting;
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

}  // namespace agent
}  // namespace volta
