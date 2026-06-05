#include "stream_data_reactor.h"

namespace volta {
namespace agent {
namespace client {

StreamMetricsReactor::StreamMetricsReactor(
    ::volta::VoltaCollector::Stub* stub, const std::string& id,
    std::shared_ptr<::volta::agent::MetricsBuffer> buffer,
    OnDoneCallback on_done)
    : on_done_(std::move(on_done)), buffer_(buffer) {
  context_.AddMetadata("agent-id", id);
  stub->async()->StreamMetrics(&context_, this);
  EnqueueMetrics();

  StartCall();
}

void StreamMetricsReactor::OnReadDone(bool ok) {
  if (!ok) {
    std::cerr << "Read failed" << std::endl;
    return;
  }

  if (msg == nullptr) {
    std::cerr << "Received null message from server" << std::endl;
    Write();
    return;
  }

  if (GetMessage(msg->batch_id()) != nullptr) {
    std::cout << "Received ack for batch_id " << msg->batch_id()
              << ", removing from readermap" << std::endl;
    UnbindMessage(msg->batch_id());
  } else {
    std::cout << "Received ack for unknown batch_id " << msg->batch_id()
              << std::endl;
  }

  std::cout << "Samples_received: " << msg->samples_received() << std::endl;

  if (msg->has_error()) {
    std::cerr << "Error: " << *msg->mutable_error() << std::endl;
  }

  EnqueueMetrics();
  Write();
}

void StreamMetricsReactor::OnWriteDone(bool ok) {
  if (!ok) {
    std::cerr << "Write failed" << std::endl;
    return;
  }

  {
    std::lock_guard l(mu_);

    writerqu_.pop();
    writing_ = false;
  }

  std::cout << "[" << std::chrono::system_clock::now() << "]"
            << " Finished writing metric to server, queue size: "
            << writerqu_.size() << std::endl;

  StartRead(msg);
}

void StreamMetricsReactor::OnDone(const grpc::Status& status) {
  on_done_(status);
}

void StreamMetricsReactor::EnqueueWrite(::volta::MetricBatch msg) {
  std::lock_guard<std::mutex> l(mu_);

  readermap_[msg.id()] = msg;
  writerqu_.push(std::move(msg));

  Write();
}

void StreamMetricsReactor::BindMessage(const uint64_t& key,
                                       ::volta::MetricBatch msg) {
  std::lock_guard<std::mutex> l(mu_);

  readermap_[key] = std::move(msg);
}

void StreamMetricsReactor::UnbindMessage(const uint64_t& key) {
  std::lock_guard<std::mutex> l(mu_);

  readermap_.erase(key);
}

void StreamMetricsReactor::Write() {
  if (writing_ || writerqu_.empty()) {
    StartRead(msg);
    return;
  }

  writing_ = true;
  auto& msg = writerqu_.front();
  StartWrite(&msg);
}

void StreamMetricsReactor::EnqueueMetrics() {
  std::vector<::volta::MetricBatch> batches;
  auto keys = buffer_->GetAllKeys();

  for (const auto& key : keys) {
    ::volta::agent::SeriesBuffer::Snapshot snapshot =
        buffer_->GetBuffer(key)->GetSnapshot();

    if (snapshot.samples.empty()) {
      continue;
    }

    ::volta::MetricBatch batch;
    for (const auto& sample : snapshot.samples) {
      batch.add_timestamps_ns(sample.timestamp_ns);
      batch.add_values(sample.value);
    }

    ::volta::BatchHeader* header = batch.mutable_header();
    header->set_metric_type(static_cast<::volta::MetricType>(key.metric_type));

    ::volta::DeviceID* device_id = header->mutable_device_id();

    if (key.pci_domain + key.pci_bus + key.pci_device + key.pci_function > 0) {
      device_id->mutable_gpu()->set_pci_domain(key.pci_domain);
      device_id->mutable_gpu()->set_pci_bus(key.pci_bus);
      device_id->mutable_gpu()->set_pci_device(key.pci_device);
      device_id->mutable_gpu()->set_pci_function(key.pci_function);
    } else if (key.socket_index + key.core_index > 0) {
      device_id->mutable_cpu()->set_socket_index(key.socket_index);
      device_id->mutable_cpu()->set_core_index(key.core_index);
    } else if (key.ifindex > 0) {
      device_id->mutable_network()->set_ifindex(key.ifindex);
    } else if (key.disk_major + key.disk_minor > 0) {
      device_id->mutable_disk()->set_major(key.disk_major);
      device_id->mutable_disk()->set_minor(key.disk_minor);
    }

    batches.push_back(std::move(batch));
  }

  if (batches.empty()) {
    return;
  }

  std::lock_guard<std::mutex> l(mu_);
  for (auto& batch : batches) {
    readermap_.emplace(batch.id(), batch);
    writerqu_.push(std::move(batch));
  }

  Write();
}

}  // namespace client
}  // namespace agent
}  // namespace volta
