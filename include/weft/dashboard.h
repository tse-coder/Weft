#pragma once

#include "weft/metrics.h"
#include <atomic>
#include <thread>
#include <vector>

namespace weft {

/**
 * @brief TUI CLI Dashboard. Runs on a background thread and polls metrics.
 */
class Dashboard {
public:
    explicit Dashboard(std::vector<WorkerMetrics*>& workers);
    ~Dashboard();

    void start();
    void stop();

private:
    void render_loop();
    
    std::vector<WorkerMetrics*>& workers_;
    std::atomic<bool> running_{false};
    std::thread render_thread_;
};

} // namespace weft
