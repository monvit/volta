#ifndef VOLTA_AGENT_CLIENT_STREAM_DATA_REACTOR_H_
#define VOLTA_AGENT_CLIENT_STREAM_DATA_REACTOR_H_

#include <mutex>
#include <random>
#include <thread>

#include "iwriter.h"
#include "volta.grpc.pb.h"

namespace volta {
namespace agent {
namespace client {

namespace {
::volta::Metric CreateRandomMetric() {
  static std::mt19937 rng{std::random_device{}()};
  static std::uniform_real_distribution<double> val_dist(0.0, 100.0);
  static std::uniform_int_distribution<int> name_dist(0, 4);
  static const std::array<std::string, 5> names = {
      "cpu_usage", "mem_usage", "disk_io", "net_rx", "net_tx"};

  ::volta::Metric metric;
  metric.set_name(names[name_dist(rng)]);
  metric.set_value(val_dist(rng));

  std::this_thread::sleep_for(std::chrono::seconds(4));

  return metric;
}
}  // namespace

class StreamDataReactor : public grpc::ClientWriteReactor<::volta::Metric>,
                          public IWriter<::volta::Metric> {
 public:
  using OnDoneCallback = std::function<void(const grpc::Status&)>;

  StreamDataReactor(::volta::VoltaCollector::Stub* stub, const std::string& id,
                    OnDoneCallback on_done);
  ~StreamDataReactor() override = default;

  // ClientWriteReactor
  void OnWriteDone(bool ok) override;
  void OnDone(const grpc::Status& status) override;

  // IWriter
  void EnqueueWrite(::volta::Metric msg) override;

  ::volta::Metric CreateMetric(const std::string& name, double value);

 private:
  void Write() override;

  OnDoneCallback on_done_;
  grpc::ClientContext context_;
  std::mutex mu_;
};

}  // namespace client
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CLIENT_STREAM_DATA_REACTOR_H_