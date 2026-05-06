#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include "weft/task.h"
#include "weft/metrics.h"
#include "weft/dashboard.h"
#include "weft/thread_pool.h"

int main() {
    std::cout << "Starting Weft CLI with Lock-Free Work Stealing (Chase-Lev)...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));

    weft::ThreadPool pool(4);
    std::vector<weft::WorkerMetrics*> metrics = pool.get_metrics();
    
    weft::Dashboard dashboard(metrics);
    dashboard.start();
    
    std::atomic<int> shared_counter{0};
    const int NUM_TASKS = 2000;
    
    // Submit tiny tasks to the pool
    for (int i = 0; i < NUM_TASKS; ++i) {
        pool.submit([&shared_counter]() {
            // Simulate variable work
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            shared_counter.fetch_add(1, std::memory_order_relaxed);
        });
        
        if (i % 200 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // stagger submissions
        }
    }
    
    // Wait for tasks to complete
    while (shared_counter.load() < NUM_TASKS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Keep dashboard open for a moment to see final stats
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    dashboard.stop();
    std::cout << "\033[2J\033[1;1H";
    std::cout << "Successfully processed " << NUM_TASKS << " tasks using baseline global queue.\n";
    
    return 0;
}
