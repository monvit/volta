#pragma once

#include <mqueue.h>

#include <functional>
#include <string>

class MessageQueue {
 public:
  enum class Role { Sender, Receiver };

  using MessageHandler = std::function<void(std::string_view)>;

  struct Options {
    long max_messages = 10;
    long max_msg_size = 4096;
    int permissions = 0660;
  };

  MessageQueue(const std::string& name, Role role, Options opts);
  ~MessageQueue();

  MessageQueue(const MessageQueue&) = delete;
  MessageQueue& operator=(const MessageQueue&) = delete;

  // Sender API

  // Blocking send. Returns false and sets errno on failure.
  bool send(std::string_view message, unsigned int priority = 0);

  // Non-blocking send. Returns false immediately if the queue is full.
  bool send_nonblocking(std::string_view message, unsigned int priority = 0);

  // Receiver API

  void listen(MessageHandler handler);

  void stop_listening();

  bool is_listening() const { return listening_; }

  long pending_count() const;

  const std::string& name() const { return name_; }

 private:
  void open_queue();
  void arm_notify();
  void drain_and_rearm();
  static void on_notify(union sigval sv);

  std::string name_;
  Role role_;
  Options opts_;
  mqd_t fd_ = static_cast<mqd_t>(-1);
  MessageHandler handler_;
  bool listening_ = false;
};
