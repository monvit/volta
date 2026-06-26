#include "message_queue.h"

#include <signal.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

MessageQueue::MessageQueue(const std::string& name, Role role,
                           const Options& opts)
    : name_(name), role_(role), opts_(opts) {
  if (name.empty() || name[0] != '/')
    throw std::invalid_argument("Queue name must start with '/'");

  open_queue();
}

MessageQueue::~MessageQueue() {
  stop_listening();
  if (fd_ != static_cast<mqd_t>(-1)) {
    mq_close(fd_);
    if (role_ == Role::Receiver) mq_unlink(name_.c_str());
  }
}

void MessageQueue::open_queue() {
  if (role_ == Role::Receiver) mq_unlink(name_.c_str());
  struct mq_attr attr{};
  attr.mq_flags = 0;
  attr.mq_maxmsg = opts_.max_messages;
  attr.mq_msgsize = opts_.max_msg_size;
  attr.mq_curmsgs = 0;

  int flags = (role_ == Role::Receiver) ? (O_RDONLY | O_CREAT | O_NONBLOCK)
                                        : (O_WRONLY | O_NONBLOCK);

  fd_ = ::mq_open(name_.c_str(), flags, opts_.permissions, &attr);

  if (fd_ == static_cast<mqd_t>(-1)) {
    throw std::runtime_error(std::string("mq_open failed for '") + name_ +
                             "': " + std::strerror(errno));
  }
}

void MessageQueue::arm_notify() {
  struct sigevent sev{};
  sev.sigev_notify = SIGEV_THREAD;
  sev.sigev_notify_function = &MessageQueue::on_notify;
  sev.sigev_notify_attributes = nullptr;  // default thread attributes
  sev.sigev_value.sival_ptr = this;

  if (mq_notify(fd_, &sev) == -1)
    throw std::runtime_error(std::string("mq_notify failed: ") +
                             std::strerror(errno));
}

void MessageQueue::drain_and_rearm() {
  std::string buf(opts_.max_msg_size, '\0');

  // Drain every message currently in the queue before re-arming.
  // mq_notify only fires on the empty->non-empty transition, so we must
  // consume everything now or we will miss messages added while we were
  // in this callback.
  while (true) {
    unsigned int priority = 0;
    ssize_t n = ::mq_receive(fd_, buf.data(), buf.size(), &priority);
    if (n == -1) {
      if (errno == EAGAIN) break;  // queue is empty — done
      listening_ = false;
      return;
    }

    if (handler_)
      handler_(std::string_view(buf.data(), static_cast<size_t>(n)));
  }

  if (listening_) arm_notify();
}

void MessageQueue::on_notify(union sigval sv) {
  auto* self = static_cast<MessageQueue*>(sv.sival_ptr);
  self->drain_and_rearm();
}

bool MessageQueue::send(std::string_view message, unsigned int priority) {
  if (role_ != Role::Sender)
    throw std::logic_error("send() called on a Receiver queue");
  if (message.size() > static_cast<size_t>(opts_.max_msg_size))
    throw std::invalid_argument("Message exceeds max_msg_size");

  return mq_send(fd_, message.data(), message.size(), priority) == 0;
}

bool MessageQueue::send_nonblocking(std::string_view message,
                                    unsigned int priority) {
  if (role_ != Role::Sender)
    throw std::logic_error("send_nonblocking() called on a Receiver queue");
  if (message.size() > static_cast<size_t>(opts_.max_msg_size))
    throw std::invalid_argument("Message exceeds max_msg_size");

  struct timespec ts = {0, 0};
  return mq_timedsend(fd_, message.data(), message.size(), priority, &ts) == 0;
}

void MessageQueue::listen(MessageHandler handler) {
  if (role_ != Role::Receiver)
    throw std::logic_error("listen() called on a Sender queue");
  if (listening_) return;

  handler_ = std::move(handler);
  listening_ = true;
  arm_notify();
}

void MessageQueue::stop_listening() {
  if (!listening_) return;
  listening_ = false;
  // Disarm any pending notification.
  mq_notify(fd_, nullptr);
  mq_unlink(name_.c_str());
}

long MessageQueue::pending_count() const {
  struct mq_attr attr{};
  if (mq_getattr(fd_, &attr) == -1)
    throw std::runtime_error(std::string("mq_getattr failed: ") +
                             std::strerror(errno));
  return attr.mq_curmsgs;
}
