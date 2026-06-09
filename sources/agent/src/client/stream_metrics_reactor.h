#ifndef VOLTA_AGENT_CLIENT_STREAM_METRICS_REACTOR_H_
#define VOLTA_AGENT_CLIENT_STREAM_METRICS_REACTOR_H_

#include <mutex>
#include <random>
#include <thread>

#include "buffer.h"
#include "ireaderwriter.h"
#include "volta.grpc.pb.h"

namespace volta {
namespace agent {
namespace client {

class StreamMetricsReactor
    : public grpc::ClientBidiReactor<::volta::MetricBatch, ::volta::BatchAck>,
      public IReaderWriter<uint64_t, ::volta::MetricBatch, ::volta::BatchAck> {
 public:
  using OnDoneCallback = std::function<void(const grpc::Status&)>;

  StreamMetricsReactor(::volta::VoltaCollector::Stub* stub,
                       const std::string& id,
                       std::shared_ptr<::volta::agent::MetricsBuffer> buffer,
                       OnDoneCallback on_done);
  ~StreamMetricsReactor() override {
    std::cout << "StreamMetricsReactor destroyed for agent "
              << context_.GetServerInitialMetadata().find("agent-id")->second
              << std::endl;
  }

  // ClientBidiReactor
  void OnReadDone(bool ok) override;
  void OnWriteDone(bool ok) override;
  void OnDone(const grpc::Status& status) override;

  // IWriter
  void EnqueueWrite(::volta::MetricBatch msg) override;

  // IReader
  void BindMessage(const uint64_t& key, ::volta::MetricBatch msg) override;
  void UnbindMessage(const uint64_t& key) override;

 private:
  void Write() override;
  void EnqueueMetrics();

  OnDoneCallback on_done_;
  grpc::ClientContext context_;
  std::mutex mu_;
  std::shared_ptr<::volta::agent::MetricsBuffer> buffer_;
  std::jthread poll_thread_;
  std::atomic<unsigned long long> batch_id_counter_{0};
};

}  // namespace client
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CLIENT_STREAM_METRICS_REACTOR_H_