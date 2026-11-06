#include "modules/performance/system_info.h"
#include "modules/performance/batch_generator.h"
#include "modules/performance/benchmark_runner.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <vector>

using namespace workbench;
using namespace workbench::performance;

int main() {
    std::cout << "======================================================================\n";
    std::cout << "       UNIFIED ENGINEERING WORKBENCH - MULTICORE BENCHMARK CLI         \n";
    std::cout << "               Task D: Thermal FEM Batch Performance                  \n";
    std::cout << "======================================================================\n\n";

    // 1. Gather and print system hardware and environment metadata
    const auto meta = get_system_metadata();
    std::cout << "--- HOST SYSTEM & COMPILER METADATA ---\n";
    std::cout << "  CPU Model Name        : " << meta.cpu_model << "\n";
    std::cout << "  Physical Core Count   : ";
    if (meta.physical_core_count > 0) {
        std::cout << meta.physical_core_count << " cores\n";
    } else {
        std::cout << "Unavailable\n";
    }
    std::cout << "  Logical Processors    : " << meta.logical_processor_count;
    if (meta.hardware_concurrency_fallback_used) {
        std::cout << " (Fallback used: hardware_concurrency reported 0)";
    }
    std::cout << " threads\n";
    std::cout << "  Total System RAM      : " << meta.total_ram_readable << "\n";
    std::cout << "  Operating System      : " << meta.os_name << "\n";
    std::cout << "  Compiler              : " << meta.compiler_name_and_version << "\n";
    std::cout << "  Build Configuration   : " << meta.build_configuration << "\n";
    std::cout << "  C++ Standard          : " << meta.cpp_standard << "\n\n";

    // 2. Configure benchmark workload
    WorkloadConfig config;
    config.random_seed = 42;
    config.case_count = 200;
    config.elements_per_case = 80;

    std::cout << "--- BENCHMARK WORKLOAD CONFIGURATION ---\n";
    std::cout << "  Thermal Simulation Cases : " << config.case_count << "\n";
    std::cout << "  Elements Per Case (N)    : " << config.elements_per_case << "\n";
    std::cout << "  Random Number Seed       : " << config.random_seed << "\n";
    std::cout << "  Warm-up Runs Per Mode    : 1 (Excluded from timing)\n";
    std::cout << "  Measured Repetitions     : 5\n";
    std::cout << "  Worker Counts Benchmarked: Serial, 1, 2, 3, 4 Workers\n";
    std::cout << "  Correctness Tolerance    : <= 1.0e-8 C absolute difference\n\n";

    std::cout << "Running benchmark repetitions, please wait...\n";

    // 3. Execute the benchmark suite
    const std::vector<size_t> worker_targets = {1, 2, 3, 4};
    const auto suite_res = run_benchmark_suite(config, worker_targets);

    if (!suite_res.is_ok()) {
        std::cerr << "\n[ERROR] Benchmark execution failed: " << suite_res.error().message << "\n";
        return 1;
    }

    const auto& suite = suite_res.value();

    std::cout << "\n======================================================================\n";
    std::cout << "                      MEASURED BENCHMARK RESULTS                       \n";
    std::cout << "======================================================================\n";

    std::cout << std::left
              << std::setw(24) << "Execution Mode"
              << std::setw(10) << "Workers"
              << std::setw(16) << "Median Time (ms)"
              << std::setw(12) << "Speedup"
              << std::setw(14) << "Efficiency"
              << std::setw(14) << "Status"
              << "\n";
    std::cout << std::string(90, '-') << "\n";

    for (const auto& sum : suite.summaries) {
        if (!sum.was_tested) {
            std::cout << std::left
                      << std::setw(24) << sum.execution_mode
                      << std::setw(10) << sum.actual_workers
                      << std::setw(16) << "N/A"
                      << std::setw(12) << "N/A"
                      << std::setw(14) << "N/A"
                      << std::setw(14) << "SKIPPED"
                      << "\n";
            continue;
        }

        std::ostringstream med_ss, spd_ss, eff_ss;
        med_ss << std::fixed << std::setprecision(2) << sum.median_time_ms << " ms";
        spd_ss << std::fixed << std::setprecision(2) << sum.speedup << "x";
        eff_ss << std::fixed << std::setprecision(2) << sum.efficiency_percent << "%";

        std::cout << std::left
                  << std::setw(24) << sum.execution_mode
                  << std::setw(10) << sum.actual_workers
                  << std::setw(16) << med_ss.str()
                  << std::setw(12) << spd_ss.str()
                  << std::setw(14) << eff_ss.str()
                  << std::setw(14) << (sum.all_repetitions_correct ? "PASSED" : "FAILED")
                  << "\n";
    }
    std::cout << std::string(90, '-') << "\n\n";

    // 4. Export CSV files
    std::filesystem::create_directories("results/benchmark");
    const std::string raw_csv_path = "results/benchmark/benchmark_raw.csv";
    const std::string sum_csv_path = "results/benchmark/benchmark_summary.csv";

    const auto raw_status = export_raw_benchmark_csv(raw_csv_path, suite);
    const auto sum_status = export_summary_benchmark_csv(sum_csv_path, suite);

    if (raw_status.is_ok() && sum_status.is_ok()) {
        std::cout << "[SUCCESS] Benchmark CSV files exported successfully:\n";
        std::cout << "  - Raw Repetitions : " << raw_csv_path << "\n";
        std::cout << "  - Summary Table   : " << sum_csv_path << "\n";
    } else {
        std::cerr << "[WARNING] Failed to export one or more CSV files:\n";
        if (raw_status.is_error()) std::cerr << "  Raw CSV error: " << raw_status.error().message << "\n";
        if (sum_status.is_error()) std::cerr << "  Summary CSV error: " << sum_status.error().message << "\n";
    }

    std::cout << "\nOverall Numerical Agreement Status: "
              << (suite.overall_correctness_passed ? "100% VERIFIED (All runs within 1e-8 C)" : "FAILED")
              << "\n======================================================================\n";

    return suite.overall_correctness_passed ? 0 : 1;
}
