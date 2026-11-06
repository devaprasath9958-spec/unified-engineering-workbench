#pragma once

#include "core/result.h"
#include "modules/thermal/fem_1d_solver.h"

#include <vector>
#include <string>
#include <cstddef>

namespace workbench::performance {

/// @brief Execution mode for batch thermal simulations.
enum class ExecutionMode {
    Serial,
    Parallel
};

/// @brief Returns a human-readable string representation of the execution mode.
inline const char* execution_mode_to_string(ExecutionMode mode) {
    switch (mode) {
        case ExecutionMode::Serial: return "Serial";
        case ExecutionMode::Parallel: return "Parallel";
        default: return "Unknown";
    }
}

/// @brief Contains the timing, metadata, and ordered results of a batch execution run.
struct BatchRunResult {
    ExecutionMode mode = ExecutionMode::Serial;
    size_t requested_workers = 1;
    size_t actual_workers = 1;
    double elapsed_time_ms = 0.0;
    std::vector<core::Result<thermal::Fem1DResult>> simulation_results;
    bool all_succeeded = false;
    size_t success_count = 0;
    size_t failure_count = 0;
    std::string error_message;
};

/// @brief Determines the safe worker count capped by host CPU hardware concurrency (with fallback to 1 if 0).
size_t resolve_worker_count(size_t requested_workers);

/// @brief Executes a batch of thermal simulations sequentially on a single thread.
/// @param workload Pre-generated vector of simulation cases.
/// @return BatchRunResult containing individual results in original input order and wall-clock execution time.
BatchRunResult execute_serial_batch(const std::vector<thermal::Thermal1DParameters>& workload);

/// @brief Executes a batch of thermal simulations in parallel using an exception-safe bounded worker pool.
/// @param workload Pre-generated vector of simulation cases.
/// @param requested_workers Number of workers requested. Capped at available hardware threads.
/// @return BatchRunResult containing individual results in original input order and wall-clock execution time.
BatchRunResult execute_parallel_batch(const std::vector<thermal::Thermal1DParameters>& workload, size_t requested_workers);

} // namespace workbench::performance
