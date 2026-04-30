#pragma once

#include <functional>

namespace weft {

/**
 * @brief Represents a single unit of work to be executed by the thread pool.
 * 
 * Using std::function introduces a small overhead (type erasure and potential heap allocation
 * for large lambdas), but provides maximum flexibility for our initial milestones.
 */
using Task = std::function<void()>;

} // namespace weft
