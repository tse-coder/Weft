#pragma once

#include "weft/task.h"
#include "weft/metrics.h"
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>

namespace weft {

/**
 * @brief A basic thread pool using a single global queue protected by a mutex.
 * This is our unoptimized baseline.
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    void submit(Task task);
    
    std::vector<WorkerMetrics*> get_metrics();

private:
    void worker_loop(size_t worker_id);

    std::vector<std::thread> threads_;
    std::vector<std::unique_ptr<WorkerMetrics>> metrics_;

    std::queue<Task> global_queue_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
};

} // namespace weft
