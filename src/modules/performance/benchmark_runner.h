#pragma once

#include "core/result.h"
#include "modules/performance/system_info.h"
#include "modules/performance/batch_generator.h"
#include "modules/performance/batch_runner.h"
#include "modules/performance/correctness_verifier.h"

#include <vector>
#include <string>

namespace workbench::performance {

/// @brief Represents a single repetition record of a benchmark execution.
struct BenchmarkRepetitionRecord {
    std::string execution_mode;
    size_t requested_workers = 1;
    size_t actual_workers = 1;
    size_t repetition_index = 0; // 1-based index (1 to 5)
    double elapsed_time_ms = 0.0;
    size_t case_count = 0;
    size_t elements_per_case = 0;
    uint32_t random_seed = 0;
    double max_temperature_difference_C = 0.0;
    bool correctness_passed = false;
};

/// @brief Summary of performance metrics for an execution mode across all repetitions.
struct BenchmarkModeSummary {
    std::string execution_mode;
    size_t requested_workers = 1;
    size_t actual_workers = 1;
    double median_time_ms = 0.0;
    double speedup = 0.0;             // T_serial / T_parallel (0.0 if correctness failed)
    double efficiency_ratio = 0.0;     // speedup / actual_workers
    double efficiency_percent = 0.0;   // efficiency_ratio * 100.0
    bool all_repetitions_correct = false;
    bool was_tested = true;
    std::string note;
};

/// @brief Complete benchmark suite outcome including metadata, repetitions, and summaries.
struct BenchmarkSuiteResult {
    SystemMetadata system_metadata;
    WorkloadConfig workload_config;
    size_t warmup_count = 1;
    size_t measured_repetitions = 5;
    std::vector<BenchmarkRepetitionRecord> raw_records;
    std::vector<BenchmarkModeSummary> summaries;
    bool overall_correctness_passed = false;
};

/// @brief Calculates the median value of a collection of numerical samples.
/// @param samples Non-empty vector of measured times.
/// @return Median value.
double calculate_median(std::vector<double> samples);

/// @brief Executes the complete 5-repetition multicore thermal simulation benchmark suite.
/// @param config Workload settings for batch generation.
/// @param worker_targets Worker counts to benchmark (default: 1, 2, 3, 4).
/// @return BenchmarkSuiteResult containing all raw and summarized performance data.
core::Result<BenchmarkSuiteResult> run_benchmark_suite(
    const WorkloadConfig& config,
    const std::vector<size_t>& worker_targets = {1, 2, 3, 4}
);

/// @brief Exports raw repetition benchmark measurements to CSV.
core::Status export_raw_benchmark_csv(
    const std::string& filepath,
    const BenchmarkSuiteResult& suite
);

/// @brief Exports summarized performance metrics (median time, speedup, efficiency) to CSV.
core::Status export_summary_benchmark_csv(
    const std::string& filepath,
    const BenchmarkSuiteResult& suite
);

} // namespace workbench::performance
