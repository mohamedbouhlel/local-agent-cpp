#pragma once

#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace tui_poc {
struct Event { unsigned long long sequence{}; std::string text; bool terminal{}; };

// Bounded experiment queue. Producers never wait for capacity. Overflow is
// explicit; the final terminal event has a separate single-slot mailbox.
class EventQueue {
 public:
  bool push(Event event) {
    std::lock_guard lock(mutex_);
    if (event.terminal) {
      terminal_ = std::move(event);
      ready_.notify_one();
      return true;
    }
    if (overflow_ || queue_.size() >= 128 || bytes_ + event.text.size() > 256 * 1024 || event.text.size() > 8192) {
      ++dropped_; overflow_ = true; ready_.notify_one(); return false;
    }
    bytes_ += event.text.size(); queue_.push_back(std::move(event));
    ready_.notify_one(); return true;
  }
  bool pop(Event& event, int wait_ms = 0) {
    std::unique_lock lock(mutex_);
    if (queue_.empty() && !terminal_ && wait_ms > 0)
      ready_.wait_for(lock, std::chrono::milliseconds(wait_ms), [&]{ return !queue_.empty() || terminal_.has_value(); });
    if (!queue_.empty()) { event = std::move(queue_.front()); bytes_ -= event.text.size(); queue_.pop_front(); return true; }
    if (terminal_) { event = std::move(*terminal_); terminal_.reset(); return true; }
    return false;
  }
  bool overflowed() const { std::lock_guard lock(mutex_); return overflow_; }
  std::size_t dropped() const { std::lock_guard lock(mutex_); return dropped_; }
 private:
  mutable std::mutex mutex_; std::condition_variable ready_; std::deque<Event> queue_;
  std::optional<Event> terminal_; std::size_t bytes_{}; std::size_t dropped_{}; bool overflow_{};
};
}
