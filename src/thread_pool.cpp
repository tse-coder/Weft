#include "weft/thread_pool.h"

namespace weft {

ThreadPool::ThreadPool(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
        metrics_.push_back(std::make_unique<WorkerMetrics>());
        queues_.push_back(std::make_unique<WorkerQueue>());
    }
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back(&ThreadPool::worker_loop, this, i);
    }
}

ThreadPool::~ThreadPool() {
    stop_ = true;
    for (auto& q : queues_) {
        q->cv.notify_all();
    }
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ThreadPool::submit(Task task) {
    size_t q_idx = next_queue_.fetch_add(1, std::memory_order_relaxed) % queues_.size();
    auto& target_queue = queues_[q_idx];
    
    {
        std::unique_lock<std::mutex> lock(target_queue->mutex);
        target_queue->queue.push(std::move(task));
    }
    target_queue->cv.notify_one();
}

std::vector<WorkerMetrics*> ThreadPool::get_metrics() {
    std::vector<WorkerMetrics*> ptrs;
    for (auto& m : metrics_) {
        ptrs.push_back(m.get());
    }
    return ptrs;
}

void ThreadPool::worker_loop(size_t worker_id) {
    WorkerMetrics* local_metrics = metrics_[worker_id].get();
    auto& local_queue = queues_[worker_id];
    size_t num_queues = queues_.size();
    
    // Simple thread-local PRNG for victim selection
    thread_local uint32_t rng_state = worker_id + 0x1337;
    auto xorshift32 = []() -> uint32_t {
        uint32_t x = rng_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rng_state = x;
        return x;
    };
    
    while (!stop_) {
        Task task;
        bool has_task = false;
        
        // 1. Try local queue
        {
            std::unique_lock<std::mutex> lock(local_queue->mutex);
            if (!local_queue->queue.empty()) {
                task = std::move(local_queue->queue.front());
                local_queue->queue.pop();
                has_task = true;
                local_metrics->local_queue_size.store(local_queue->queue.size(), std::memory_order_relaxed);
            }
        }
        
        // 2. Try stealing if local is empty
        if (!has_task && num_queues > 1) {
            local_metrics->steal_attempts.fetch_add(1, std::memory_order_relaxed);
            size_t start_idx = xorshift32() % num_queues;
            
            for (size_t i = 0; i < num_queues; ++i) {
                size_t victim_idx = (start_idx + i) % num_queues;
                if (victim_idx == worker_id) continue;
                
                auto& victim_queue = queues_[victim_idx];
                
                // use try_lock to avoid blocking and deadlocks
                std::unique_lock<std::mutex> lock(victim_queue->mutex, std::try_to_lock);
                if (lock.owns_lock() && !victim_queue->queue.empty()) {
                    task = std::move(victim_queue->queue.front());
                    victim_queue->queue.pop();
                    has_task = true;
                    local_metrics->successful_steals.fetch_add(1, std::memory_order_relaxed);
                    break;
                }
            }
        }
        
        // 3. Execute or wait
        if (has_task) {
            task();
            local_metrics->tasks_completed.fetch_add(1, std::memory_order_relaxed);
        } else {
            // Wait for work on our local queue with a timeout (to occasionally re-try stealing)
            std::unique_lock<std::mutex> lock(local_queue->mutex);
            local_queue->cv.wait_for(lock, std::chrono::milliseconds(1), [this, &local_queue] {
                return stop_ || !local_queue->queue.empty();
            });
        }
    }
}

} // namespace weft
