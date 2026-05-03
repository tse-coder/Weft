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
    // Round-robin submission
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
    
    while (!stop_) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(local_queue->mutex);
            local_queue->cv.wait(lock, [this, &local_queue] { 
                return stop_ || !local_queue->queue.empty(); 
            });
            
            if (stop_ && local_queue->queue.empty()) {
                return;
            }
            
            task = std::move(local_queue->queue.front());
            local_queue->queue.pop();
            
            local_metrics->local_queue_size.store(local_queue->queue.size(), std::memory_order_relaxed);
        }
        
        task();
        
        local_metrics->tasks_completed.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace weft
