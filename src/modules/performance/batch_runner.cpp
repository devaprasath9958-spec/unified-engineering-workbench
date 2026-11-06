#include "modules/performance/batch_runner.h"

#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>

namespace workbench::performance {

size_t resolve_worker_count(size_t requested_workers) {
    const unsigned int hw = std::thread::hardware_concurrency();
    const size_t max_hw = (hw == 0) ? 1 : static_cast<size_t>(hw);
    const size_t safe_req = (requested_workers == 0) ? 1 : requested_workers;
    return std::min(safe_req, max_hw);
}

BatchRunResult execute_serial_batch(const std::vector<thermal::Thermal1DParameters>& workload) {
    BatchRunResult result;
    result.mode = ExecutionMode::Serial;
    result.requested_workers = 1;
    result.actual_workers = 1;

    if (workload.empty()) {
        result.all_succeeded = true;
        result.elapsed_time_ms = 0.0;
        return result;
    }

    result.simulation_results.assign(
        workload.size(),
        core::Result<thermal::Fem1DResult>::failure(core::StatusCode::ComputationError, "Uninitialized")
    );

    const auto t_start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < workload.size(); ++i) {
        try {
            result.simulation_results[i] = thermal::solve_steady_state_1d(workload[i]);
        } catch (const std::exception& ex) {
            result.simulation_results[i] = core::Result<thermal::Fem1DResult>::failure(
                core::StatusCode::ComputationError,
                std::string("Exception during serial thermal solve: ") + ex.what()
            );
        } catch (...) {
            result.simulation_results[i] = core::Result<thermal::Fem1DResult>::failure(
                core::StatusCode::ComputationError,
                "Unknown exception during serial thermal solve."
            );
        }

        if (result.simulation_results[i].is_ok()) {
            result.success_count++;
        } else {
            result.failure_count++;
        }
    }

    const auto t_end = std::chrono::steady_clock::now();
    result.elapsed_time_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    result.all_succeeded = (result.failure_count == 0);

    return result;
}

BatchRunResult execute_parallel_batch(const std::vector<thermal::Thermal1DParameters>& workload, size_t requested_workers) {
    BatchRunResult result;
    result.mode = ExecutionMode::Parallel;
    result.requested_workers = requested_workers;
    result.actual_workers = resolve_worker_count(requested_workers);

    if (workload.empty()) {
        result.all_succeeded = true;
        result.elapsed_time_ms = 0.0;
        return result;
    }

    result.simulation_results.assign(
        workload.size(),
        core::Result<thermal::Fem1DResult>::failure(core::StatusCode::ComputationError, "Uninitialized")
    );

    std::atomic<size_t> next_index{0};

    auto worker_task = [&]() {
        while (true) {
            const size_t idx = next_index.fetch_add(1, std::memory_order_relaxed);
            if (idx >= workload.size()) {
                break;
            }

            try {
                result.simulation_results[idx] = thermal::solve_steady_state_1d(workload[idx]);
            } catch (const std::exception& ex) {
                result.simulation_results[idx] = core::Result<thermal::Fem1DResult>::failure(
                    core::StatusCode::ComputationError,
                    std::string("Exception during worker thermal solve: ") + ex.what()
                );
            } catch (...) {
                result.simulation_results[idx] = core::Result<thermal::Fem1DResult>::failure(
                    core::StatusCode::ComputationError,
                    "Unknown exception during worker thermal solve."
                );
            }
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(result.actual_workers);

    const auto t_start = std::chrono::steady_clock::now();

    try {
        for (size_t w = 0; w < result.actual_workers; ++w) {
            workers.emplace_back(worker_task);
        }
    } catch (const std::exception& ex) {
        // Exception safety: join all already-started threads before propagating failure
        for (auto& th : workers) {
            if (th.joinable()) {
                th.join();
            }
        }
        const auto t_fail_end = std::chrono::steady_clock::now();
        result.elapsed_time_ms = std::chrono::duration<double, std::milli>(t_fail_end - t_start).count();
        result.all_succeeded = false;
        result.error_message = std::string("Thread creation failed: ") + ex.what();
        return result;
    } catch (...) {
        for (auto& th : workers) {
            if (th.joinable()) {
                th.join();
            }
        }
        const auto t_fail_end = std::chrono::steady_clock::now();
        result.elapsed_time_ms = std::chrono::duration<double, std::milli>(t_fail_end - t_start).count();
        result.all_succeeded = false;
        result.error_message = "Unknown error occurred during thread creation.";
        return result;
    }

    // Join all worker threads
    for (auto& th : workers) {
        if (th.joinable()) {
            th.join();
        }
    }

    const auto t_end = std::chrono::steady_clock::now();
    result.elapsed_time_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    // Aggregate successes and failures
    for (const auto& sim_res : result.simulation_results) {
        if (sim_res.is_ok()) {
            result.success_count++;
        } else {
            result.failure_count++;
        }
    }
    result.all_succeeded = (result.failure_count == 0);

    return result;
}

} // namespace workbench::performance
