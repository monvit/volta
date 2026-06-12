#ifndef VOLTA_AGENT_CLIENT_STREAM_METRICS_REACTOR_H_
#define VOLTA_AGENT_CLIENT_STREAM_METRICS_REACTOR_H_

#include <atomic>
#include <mutex>
#include <queue>
#include <unordered_map>

#include "buffer.h"
#include "volta.grpc.pb.h"

namespace volta {
namespace agent {
namespace client {

using namespace ::volta::agent;

class StreamMetricsReactor
    : public grpc::ClientBidiReactor<::volta::MetricBatch, ::volta::BatchAck> {
 public:
  using OnDoneCallback = std::function<void(const grpc::Status&)>;

  StreamMetricsReactor(::volta::VoltaCollector::Stub* stub,
                       const std::string& id,
                       std::shared_ptr<::volta::agent::MetricsBuffer> buffer,
                       OnDoneCallback on_done);
  ~StreamMetricsReactor() override = default;

  // ClientBidiReactor
  void OnReadDone(bool ok) override;
  void OnWriteDone(bool ok) override;
  void OnDone(const grpc::Status& status) override;

 private:
  using BatchId = uint64_t;

  void Write();
  void EnqueueMetrics();
  void EnqueueMetrics(const BufferKey& key);

  OnDoneCallback on_done_;
  grpc::ClientContext context_;
  std::shared_ptr<MetricsBuffer> buffer_;

  std::jthread poll_thread_;
  std::unordered_map<BatchId, std::pair<BufferKey, SeriesBuffer::Snapshot>>
      pending_snapshots_;
  std::mutex snapshot_mu_;

  std::queue<::volta::MetricBatch> writerqu_;
  bool writing_ = false;
  std::mutex write_mu_;

  ::volta::BatchAck ack_msg_;

  std::atomic<BatchId> batch_id_counter_{0};
};

}  // namespace client
}  // namespace agent
}  // namespace volta

#endif  // VOLTA_AGENT_CLIENT_STREAM_METRICS_REACTOR_H_