#pragma once

#include "weft/task.h"
#include "weft/metrics.h"
#include "weft/chase_lev_deque.h"
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>

namespace weft {

/**
 * @brief Fully lock-free ThreadPool.
 * Utilizes the Chase-Lev deque for zero-contention task stealing.
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    void submit(Task task);
    
    std::vector<WorkerMetrics*> get_metrics();

private:
    void worker_loop(size_t worker_id);

    struct WorkerState {
        WorkStealingDeque deque;
        
        // Used purely for parking/sleeping when completely idle to save CPU.
        // NOT used for queue access lock.
        std::mutex park_mutex;
        std::condition_variable park_cv;
        std::atomic<bool> sleeping{false};
    };

    std::vector<std::thread> threads_;
    std::vector<std::unique_ptr<WorkerMetrics>> metrics_;
    std::vector<std::unique_ptr<WorkerState>> workers_;

    std::atomic<size_t> next_worker_{0};
    std::atomic<bool> stop_{false};
};

} // namespace weft
