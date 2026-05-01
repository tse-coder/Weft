#pragma once

#include <atomic>
#include <cstdint>
#include <new>

namespace weft {

// Hardware constructive interference size (typically 64 bytes on x86)
#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t CACHE_LINE_SIZE = std::hardware_constructive_interference_size;
#else
    constexpr std::size_t CACHE_LINE_SIZE = 64;
#endif

/**
 * @brief Thread-local metrics. 
 * Aligned to a cache line to prevent false sharing between workers writing metrics 
 * and the dashboard thread reading them.
 */
struct alignas(CACHE_LINE_SIZE) WorkerMetrics {
    std::atomic<uint64_t> tasks_completed{0};
    std::atomic<uint64_t> steal_attempts{0};
    std::atomic<uint64_t> successful_steals{0};
    std::atomic<uint64_t> local_queue_size{0};
};

} // namespace weft
