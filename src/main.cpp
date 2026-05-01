#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include "weft/task.h"
#include "weft/metrics.h"
#include "weft/dashboard.h"

int main() {
    // Simulate some workers for our UI test
    weft::WorkerMetrics w0, w1, w2, w3;
    std::vector<weft::WorkerMetrics*> workers = {&w0, &w1, &w2, &w3};
    
    weft::Dashboard dashboard(workers);
    dashboard.start();
    
    // Simulate work happening across threads to test the CLI
    for (int i = 0; i < 30; ++i) {
        w0.tasks_completed.fetch_add(15, std::memory_order_relaxed);
        w0.local_queue_size.store(100 - i*3, std::memory_order_relaxed);
        
        w1.tasks_completed.fetch_add(5, std::memory_order_relaxed);
        w1.local_queue_size.store(50 - i, std::memory_order_relaxed);
        
        w2.successful_steals.fetch_add(1, std::memory_order_relaxed);
        w2.tasks_completed.fetch_add(1, std::memory_order_relaxed);
        
        w3.successful_steals.fetch_add(2, std::memory_order_relaxed);
        w3.tasks_completed.fetch_add(2, std::memory_order_relaxed);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    dashboard.stop();
    // Clear screen on exit
    std::cout << "\033[2J\033[1;1H";
    std::cout << "Weft simulation complete. Exiting.\n";
    
    return 0;
}
