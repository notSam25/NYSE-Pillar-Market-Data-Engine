#pragma once
#include "message.hpp"
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stop_token>

namespace mde {

// Thread-safe hand-off queue between MDE's Ingest thread and Egress thread
class MessageQueue {
public:
  void Push(Message message) {
    {
      std::lock_guard lock(_mutex);
      _queue.push(std::move(message));
    }
    _cv.notify_one();
  }

  // Blocks until a message is available, the queue has been Close()'d and
  // drained, or `stopToken` is signaled. Returns std::nullopt in the
  // latter two cases so callers can exit their loop uniformly
  std::optional<Message> WaitPop(std::stop_token stopToken) {
    std::unique_lock lock(_mutex);
    _cv.wait(lock, stopToken, [this] { return !_queue.empty() || _closed; });

    if (_queue.empty()) {
      return std::nullopt;
    }

    Message message = std::move(_queue.front());
    _queue.pop();
    return message;
  }

  // Marks the queue as finished accepting new messages and wakes any
  // thread blocked in WaitPop() so it can drain what remains and exit.
  // Idempotent by nature
  void Close() {
    {
      std::lock_guard lock(_mutex);
      _closed = true;
    }
    _cv.notify_all();
  }

  std::size_t Size() const {
    std::lock_guard lock(_mutex);
    return _queue.size();
  }

private:
  mutable std::mutex _mutex;
  std::condition_variable_any _cv;
  std::queue<Message> _queue;
  bool _closed = false;
};

} // namespace mde
