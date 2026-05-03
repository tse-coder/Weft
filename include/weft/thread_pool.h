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
 * @brief Thread pool with per-thread queues and round-robin task submission.
 * This eliminates the single global bottleneck lock.
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    void submit(Task task);
    
    std::vector<WorkerMetrics*> get_metrics();

private:
    void worker_loop(size_t worker_id);

    struct WorkerQueue {
        std::queue<Task> queue;
        std::mutex mutex;
        std::condition_variable cv;
    };

    std::vector<std::thread> threads_;
    std::vector<std::unique_ptr<WorkerMetrics>> metrics_;
    std::vector<std::unique_ptr<WorkerQueue>> queues_;

    std::atomic<size_t> next_queue_{0};
    std::atomic<bool> stop_{false};
};

} // namespace weft
