#include "weft/thread_pool.h"

namespace weft {

ThreadPool::ThreadPool(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
        metrics_.push_back(std::make_unique<WorkerMetrics>());
    }
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back(&ThreadPool::worker_loop, this, i);
    }
}

ThreadPool::~ThreadPool() {
    stop_ = true;
    condition_.notify_all();
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ThreadPool::submit(Task task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        global_queue_.push(std::move(task));
    }
    condition_.notify_one();
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
    
    while (!stop_) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] { return stop_ || !global_queue_.empty(); });
            
            if (stop_ && global_queue_.empty()) {
                return;
            }
            
            task = std::move(global_queue_.front());
            global_queue_.pop();
            
            // For the dashboard: since it's a global queue, just report it as local queue size 
            // for visualizing load, even though it's technically shared.
            local_metrics->local_queue_size.store(global_queue_.size(), std::memory_order_relaxed);
        }
        
        task();
        
        local_metrics->tasks_completed.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace weft
