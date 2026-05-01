#include "weft/dashboard.h"
#include <iostream>
#include <chrono>

namespace weft {

Dashboard::Dashboard(std::vector<WorkerMetrics*>& workers) 
    : workers_(workers) {}

Dashboard::~Dashboard() {
    stop();
}

void Dashboard::start() {
    if (!running_.exchange(true)) {
        render_thread_ = std::thread(&Dashboard::render_loop, this);
    }
}

void Dashboard::stop() {
    if (running_.exchange(false)) {
        if (render_thread_.joinable()) {
            render_thread_.join();
        }
    }
}

void Dashboard::render_loop() {
    while (running_.load(std::memory_order_relaxed)) {
        // ANSI escape sequences: 2J clears screen, 1;1H moves cursor to top-left
        std::cout << "\033[2J\033[1;1H";
        std::cout << "========================================\n";
        std::cout << "       WEFT THREAD POOL DASHBOARD       \n";
        std::cout << "========================================\n\n";

        uint64_t total_completed = 0;
        uint64_t total_steals = 0;
        
        for (size_t i = 0; i < workers_.size(); ++i) {
            uint64_t completed = workers_[i]->tasks_completed.load(std::memory_order_relaxed);
            uint64_t steals = workers_[i]->successful_steals.load(std::memory_order_relaxed);
            uint64_t q_size = workers_[i]->local_queue_size.load(std::memory_order_relaxed);
            
            total_completed += completed;
            total_steals += steals;
            
            std::cout << "Worker [" << i << "]: " 
                      << "Q-Depth: " << q_size << "\t| "
                      << "Tasks: " << completed << "\t| "
                      << "Steals: " << steals << "\n";
        }
        
        std::cout << "----------------------------------------\n";
        std::cout << "TOTAL TASKS: " << total_completed << "\t| "
                  << "TOTAL STEALS: " << total_steals << "\n\n";
        
        // Render at ~10 FPS
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace weft
