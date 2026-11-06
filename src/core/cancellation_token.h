#pragma once

#include <atomic>

namespace workbench::core {

/**
 * @brief Thread-safe cooperative cancellation token.
 * 
 * Used by engineering computations (mesh generation, solvers, benchmarks)
 * to periodically check if an abort has been requested.
 */
class CancellationToken {
public:
    CancellationToken() : m_cancelled(false) {}

    // Requests cooperative cancellation
    void request_cancellation() noexcept {
        m_cancelled.store(true, std::memory_order_relaxed);
    }

    // Resets the cancellation state back to active/running
    void reset() noexcept {
        m_cancelled.store(false, std::memory_order_relaxed);
    }

    // Checks whether cancellation has been requested
    bool is_cancelled() const noexcept {
        return m_cancelled.load(std::memory_order_relaxed);
    }

private:
    std::atomic<bool> m_cancelled;
};

} // namespace workbench::core
