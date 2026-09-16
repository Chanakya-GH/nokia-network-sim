#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

namespace netsim {

// A fixed-capacity, thread-safe FIFO queue used as the inter-node "wire"
// between simulated network nodes. Models the finite buffer memory of real
// network hardware: producers block (or fail, depending on the caller's
// choice of push vs try_push) once the queue is full, giving natural
// backpressure instead of unbounded memory growth.
//
// Design notes (interview talking points):
//  - Single mutex + two condition variables (not_full / not_empty) rather
//    than a lock-free ring buffer: simplicity and correctness first for a
//    portfolio project; a lock-free SPSC ring buffer is a natural "next
//    step" extension to discuss.
//  - close() lets producers signal "no more items" so consumer threads can
//    drain remaining items and then exit cleanly, instead of blocking
//    forever on an empty queue during shutdown.
template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

    // Blocks if the queue is full until space is available or the queue is
    // closed. Returns false if the queue was closed before space opened up
    // (i.e. the item was NOT enqueued).
    bool push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return items_.size() < capacity_ || closed_; });
        if (closed_) {
            return false;
        }
        items_.push_back(std::move(item));
        lock.unlock();
        not_empty_.notify_one();
        return true;
    }

    // Non-blocking push. Returns false immediately if the queue is full or
    // closed. Used to simulate a node that drops packets under overload
    // instead of applying backpressure upstream.
    bool try_push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_ || items_.size() >= capacity_) {
            return false;
        }
        items_.push_back(std::move(item));
        not_empty_.notify_one();
        return true;
    }

    // Blocks until an item is available or the queue is closed AND drained.
    // Returns std::nullopt only once closed and empty (the shutdown signal
    // for consumer threads).
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return !items_.empty() || closed_; });
        if (items_.empty()) {
            return std::nullopt;
        }
        T item = std::move(items_.front());
        items_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return item;
    }

    // Signals no more items will be pushed. Wakes any blocked push()/pop()
    // callers so threads can observe shutdown and exit their loops.
    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }

    std::size_t capacity() const { return capacity_; }

    bool is_closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    std::deque<T> items_;
    std::size_t capacity_;
    bool closed_ = false;
};

}  // namespace netsim
