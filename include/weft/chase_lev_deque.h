#pragma once

#include "weft/task.h"
#include <atomic>
#include <vector>
#include <optional>

namespace weft {

/**
 * @brief Lock-free Chase-Lev Work-Stealing Deque.
 * 
 * Top is accessed by stealers (FIFO) and the owner (when the queue has 1 item).
 * Bottom is accessed only by the owner (LIFO).
 */
class WorkStealingDeque {
public:
    explicit WorkStealingDeque(size_t capacity = 1024) 
        : buffer_(capacity) {
        top_.store(0, std::memory_order_relaxed);
        bottom_.store(0, std::memory_order_relaxed);
    }

    // Called only by the owner thread
    void push(Task task) {
        int64_t b = bottom_.load(std::memory_order_relaxed);
        int64_t t = top_.load(std::memory_order_acquire);
        
        if (b - t >= static_cast<int64_t>(buffer_.size()) - 1) {
            // Queue full; in a production system we'd resize, 
            // but fixed capacity is sufficient for this milestone.
            return;
        }
        
        buffer_[b % buffer_.size()] = std::move(task);
        
        // Ensure the task is fully written before bottom is updated
        std::atomic_thread_fence(std::memory_order_release);
        bottom_.store(b + 1, std::memory_order_relaxed);
    }

    // Called only by the owner thread
    std::optional<Task> pop() {
        int64_t b = bottom_.load(std::memory_order_relaxed) - 1;
        bottom_.store(b, std::memory_order_relaxed);
        
        std::atomic_thread_fence(std::memory_order_seq_cst);
        
        int64_t t = top_.load(std::memory_order_relaxed);
        
        std::optional<Task> result;
        if (t <= b) {
            // Non-empty queue
            result = std::move(buffer_[b % buffer_.size()]);
            if (t == b) {
                // Last element, potential race with stealer
                if (!top_.compare_exchange_strong(t, t + 1, 
                                                  std::memory_order_seq_cst, 
                                                  std::memory_order_relaxed)) {
                    // Failed race, stealer got it
                    result = std::nullopt;
                }
                bottom_.store(b + 1, std::memory_order_relaxed);
            }
        } else {
            // Empty queue
            bottom_.store(b + 1, std::memory_order_relaxed);
        }
        
        return result;
    }

    // Called by thief threads
    std::optional<Task> steal() {
        int64_t t = top_.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        int64_t b = bottom_.load(std::memory_order_acquire);
        
        std::optional<Task> result;
        if (t < b) {
            result = buffer_[t % buffer_.size()];
            if (!top_.compare_exchange_strong(t, t + 1, 
                                              std::memory_order_seq_cst, 
                                              std::memory_order_relaxed)) {
                // Failed race with owner or another stealer
                result = std::nullopt;
            }
        }
        
        return result;
    }
    
    size_t size() const {
        int64_t b = bottom_.load(std::memory_order_relaxed);
        int64_t t = top_.load(std::memory_order_relaxed);
        return static_cast<size_t>(std::max(int64_t(0), b - t));
    }

private:
    std::atomic<int64_t> top_;
    std::atomic<int64_t> bottom_;
    std::vector<Task> buffer_;
};

} // namespace weft
