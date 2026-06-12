#include "stream_metrics_reactor.h"

namespace volta {
namespace agent {
namespace client {

StreamMetricsReactor::StreamMetricsReactor(
    ::volta::VoltaCollector::Stub* stub, const std::string& id,
    std::shared_ptr<::volta::agent::MetricsBuffer> buffer,
    OnDoneCallback on_done)
    : on_done_(std::move(on_done)), buffer_(buffer) {
  std::cout << "Starting StreamMetricsReactor for agent " << id << std::endl;
  context_.AddMetadata("agent-id", id);
  stub->async()->StreamMetrics(&context_, this);

  StartCall();
  StartRead(&ack_msg_);

  poll_thread_ = std::jthread([this](std::stop_token st) {
    // TODO: we should consider using a more efficient synchronization mechanism
    // to avoid polling in a loop like this, e.g. condition variable or similar
    // to wake up immediately when new metrics are available or when an ACK is
    // received, instead of just sleeping for a fixed interval.
    while (!st.stop_requested()) {
      if (st.stop_requested()) break;

      bool has_pending = false;
      {
        std::lock_guard l(snapshot_mu_);
        has_pending = !pending_snapshots_.empty();
      }

      if (!has_pending) {
        EnqueueMetrics();

        {
          std::lock_guard l(snapshot_mu_);
          has_pending = !pending_snapshots_.empty();
        }

        if (!has_pending) {
          // No metrics in the buffer, wait a bit before polling again.
          // TODO: use condition variable or similar to wake up immediately when
          // new metrics are available???
          std::this_thread::sleep_for(std::chrono::seconds(1));
        } else {
          Write();
        }
        continue;
      }

      // TODO: better handling of polling interval, maybe dynamic based on how
      // fast metrics are produced and acked?
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }
  });
}

void StreamMetricsReactor::OnReadDone(bool ok) {
  if (!ok) {
    std::cerr << "Read failed" << std::endl;
    return;
  }

  BatchId batch_id = ack_msg_.batch_id();
  std::cout << "Received ACK for batch_id " << batch_id << std::endl;

  {
    std::lock_guard l(snapshot_mu_);
    auto it = pending_snapshots_.find(batch_id);
    if (it != pending_snapshots_.end()) {
      auto& [key, snapshot] = it->second;
      buffer_->GetBuffer(key)->AckSnapshot(snapshot);
      pending_snapshots_.erase(it);
      // std::cout << "Acknowledged batch_id " << batch_id << ", pending
      // snapshots: " << pending_snapshots_.size()
      //           << std::endl;
    } else {
      // TODO: Server may have returned an ACK before we added the snapshot to
      // pending_snapshots_, so this might be a normal occurrence. We should
      // consider adding some buffering or delay to handle this case more
      // gracefully instead of just printing an error.
      std::cout << "Received ACK for unknown batch_id " << batch_id
                << std::endl;
    }
  }

  if (ack_msg_.has_error()) {
    std::cerr << "Error: " << ack_msg_.error() << std::endl;
  }

  StartRead(&ack_msg_);
}

void StreamMetricsReactor::OnWriteDone(bool ok) {
  if (!ok) {
    std::cerr << "Write failed" << std::endl;
    return;
  }

  {
    std::lock_guard l(write_mu_);
    writerqu_.pop();
    writing_ = false;
  }

  // std::cout << "Finished writing batch to server, queue size: " <<
  // writerqu_.size() << " " << pending_snapshots_.size()
  //           << std::endl;

  Write();
}

void StreamMetricsReactor::OnDone(const grpc::Status& status) {
  std::cout << "StreamMetricsReactor finished: " << status.error_message()
            << std::endl;
  on_done_(status);
}

void StreamMetricsReactor::Write() {
  std::lock_guard<std::mutex> l(write_mu_);
  if (writing_ || writerqu_.empty()) {
    return;
  }

  writing_ = true;
  auto& msg = writerqu_.front();
  StartWrite(&msg);
}

void StreamMetricsReactor::EnqueueMetrics() {
  auto keys = buffer_->GetAllKeys();

  for (const auto& key : keys) {
    EnqueueMetrics(key);
  }
}

void StreamMetricsReactor::EnqueueMetrics(const BufferKey& key) {
  ::volta::agent::SeriesBuffer::Snapshot snapshot =
      buffer_->GetBuffer(key)->GetSnapshot();

  if (snapshot.samples.empty()) {
    return;
  }

  ::volta::MetricBatch batch;
  for (const auto& sample : snapshot.samples) {
    batch.add_timestamps_ns(sample.timestamp_ns);
    batch.add_values(sample.value);
  }

  ::volta::BatchHeader* header = batch.mutable_header();
  header->set_metric_type(static_cast<::volta::MetricType>(key.metric_type));

  ::volta::DeviceID* device_id = header->mutable_device_id();

  if (key.metric_type >= 100 && key.metric_type < 200) {
    device_id->mutable_cpu()->set_socket_index(key.socket_index);
    device_id->mutable_cpu()->set_core_index(key.core_index);
  } else if (key.metric_type >= 200 && key.metric_type < 300) {
    device_id->mutable_gpu()->set_pci_domain(key.pci_domain);
    device_id->mutable_gpu()->set_pci_bus(key.pci_bus);
    device_id->mutable_gpu()->set_pci_device(key.pci_device);
    device_id->mutable_gpu()->set_pci_function(key.pci_function);
  } else if (key.metric_type >= 400 && key.metric_type < 500) {
    device_id->mutable_disk()->set_major(key.disk_major);
    device_id->mutable_disk()->set_minor(key.disk_minor);
  } else if (key.metric_type >= 500 && key.metric_type < 600) {
    device_id->mutable_network()->set_ifindex(key.ifindex);
  }

  batch.set_id(batch_id_counter_.fetch_add(1));

  {
    std::lock_guard l(snapshot_mu_);
    pending_snapshots_.emplace(batch.id(), std::make_pair(key, snapshot));
  }

  std::lock_guard<std::mutex> l(write_mu_);
  writerqu_.push(std::move(batch));
}

}  // namespace client
}  // namespace agent
}  // namespace volta
