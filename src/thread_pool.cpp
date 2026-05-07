#include "weft/thread_pool.h"
#include <pthread.h>

namespace weft {

ThreadPool::ThreadPool(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
        metrics_.push_back(std::make_unique<WorkerMetrics>());
        workers_.push_back(std::make_unique<WorkerState>());
    }
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back(&ThreadPool::worker_loop, this, i);
    }
}

ThreadPool::~ThreadPool() {
    stop_ = true;
    for (auto& w : workers_) {
        w->park_cv.notify_all();
    }
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ThreadPool::submit(Task task) {
    size_t w_idx = next_worker_.fetch_add(1, std::memory_order_relaxed) % workers_.size();
    auto& target_worker = workers_[w_idx];
    
    // Completely lock-free push
    target_worker->deque.push(std::move(task));
    
    // Wake up the worker if it was sleeping
    if (target_worker->sleeping.load(std::memory_order_relaxed)) {
        target_worker->park_cv.notify_one();
    }
}

std::vector<WorkerMetrics*> ThreadPool::get_metrics() {
    std::vector<WorkerMetrics*> ptrs;
    for (auto& m : metrics_) {
        ptrs.push_back(m.get());
    }
    return ptrs;
}

void ThreadPool::worker_loop(size_t worker_id) {
    // Pin to core to maximize cache locality and reduce context switching
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(worker_id % std::thread::hardware_concurrency(), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    WorkerMetrics* local_metrics = metrics_[worker_id].get();
    auto& local_state = workers_[worker_id];
    size_t num_workers = workers_.size();
    
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
        // 1. Lock-free pop from our own bottom
        std::optional<Task> opt_task = local_state->deque.pop();
        
        if (opt_task) {
            local_metrics->local_queue_size.store(local_state->deque.size(), std::memory_order_relaxed);
        } else {
            // 2. Lock-free steal from others' top
            if (num_workers > 1) {
                local_metrics->steal_attempts.fetch_add(1, std::memory_order_relaxed);
                size_t start_idx = xorshift32() % num_workers;
                
                for (size_t i = 0; i < num_workers; ++i) {
                    size_t victim_idx = (start_idx + i) % num_workers;
                    if (victim_idx == worker_id) continue;
                    
                    auto& victim_state = workers_[victim_idx];
                    opt_task = victim_state->deque.steal();
                    
                    if (opt_task) {
                        local_metrics->successful_steals.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                }
            }
        }
        
        // 3. Execute or Park
        if (opt_task) {
            (*opt_task)();
            local_metrics->tasks_completed.fetch_add(1, std::memory_order_relaxed);
        } else {
            // No work found anywhere. Park the thread to save CPU.
            local_state->sleeping.store(true, std::memory_order_relaxed);
            
            // Double check to prevent lost wakeups before sleeping
            if (local_state->deque.size() == 0) {
                std::unique_lock<std::mutex> lock(local_state->park_mutex);
                local_state->park_cv.wait_for(lock, std::chrono::milliseconds(1), [this, &local_state] {
                    return stop_ || local_state->deque.size() > 0;
                });
            }
            
            local_state->sleeping.store(false, std::memory_order_relaxed);
        }
    }
}

} // namespace weft
