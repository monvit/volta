#include "stream_data_reactor.h"

namespace volta {
namespace agent {
namespace client {

StreamDataReactor::StreamDataReactor(::volta::VoltaCollector::Stub* stub, OnDoneCallback on_done)
  : on_done_(std::move(on_done)) {
  stub->async()->StreamData(&context_, nullptr, this);

  EnqueueWrite(CreateMetric("example_metric", 42.0));
  StartCall();
}

void StreamDataReactor::OnWriteDone(bool ok) {
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
            << " Finished writing metric to server, queue size: " << writerqu_.size() << std::endl;

  EnqueueWrite(CreateRandomMetric()); // for testing
  // Write();
}

void StreamDataReactor::OnDone(const grpc::Status& status) {
  on_done_(status);
}

void StreamDataReactor::EnqueueWrite(::volta::Metric msg) {
  std::lock_guard<std::mutex> l(mu_);

  writerqu_.push(std::move(msg));
  Write();
}

::volta::Metric StreamDataReactor::CreateMetric(const std::string& name, double value) {
  ::volta::Metric metric;
  metric.set_name(name);
  metric.set_value(value);
  return metric;
}

void StreamDataReactor::Write() {
  if (writing_ || writerqu_.empty()) {
    return;
  }

  writing_ = true;
  auto& msg = writerqu_.front();
  StartWrite(&msg);
}

} // namespace client
} // namespace agent
} // namespace volta
