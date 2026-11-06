#include "modules/performance/system_info.h"
#include "modules/performance/batch_generator.h"
#include "modules/performance/batch_runner.h"
#include "modules/performance/correctness_verifier.h"
#include "modules/performance/benchmark_runner.h"

#include <iostream>
#include <fstream>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <thread>

using namespace workbench;
using namespace workbench::performance;

static int g_perf_passed = 0;
static int g_perf_failed = 0;

#define PERF_ASSERT(cond, msg) \
    do { \
        if (cond) { \
            g_perf_passed++; \
        } else { \
            g_perf_failed++; \
            std::cerr << "  [FAIL] Line " << __LINE__ << ": " << (msg) << "\n"; \
        } \
    } while (0)

// 1. Fixed-seed reproducibility
void test_fixed_seed_reproducibility() {
    std::cout << "[TEST GROUP 1] Fixed-Seed Reproducibility...\n";

    WorkloadConfig config_a;
    config_a.random_seed = 12345;
    config_a.case_count = 15;
    config_a.elements_per_case = 20;

    WorkloadConfig config_b = config_a;

    const auto res_a = generate_batch_workload(config_a);
    const auto res_b = generate_batch_workload(config_b);

    PERF_ASSERT(res_a.is_ok(), "Workload A generation succeeded");
    PERF_ASSERT(res_b.is_ok(), "Workload B generation succeeded");

    const auto& vec_a = res_a.value();
    const auto& vec_b = res_b.value();
    PERF_ASSERT(vec_a.size() == 15, "Workload A size matches case_count");
    PERF_ASSERT(vec_b.size() == 15, "Workload B size matches case_count");

    bool all_identical = true;
    for (size_t i = 0; i < 15; ++i) {
        if (vec_a[i].length != vec_b[i].length ||
            vec_a[i].area != vec_b[i].area ||
            vec_a[i].thermal_conductivity != vec_b[i].thermal_conductivity ||
            vec_a[i].heat_generation != vec_b[i].heat_generation ||
            vec_a[i].left_temperature != vec_b[i].left_temperature ||
            vec_a[i].right_temperature != vec_b[i].right_temperature) {
            all_identical = false;
            break;
        }
    }
    PERF_ASSERT(all_identical, "Identical seeds produce bitwise identical thermal parameters");

    // Different seed produces different parameters
    WorkloadConfig config_c = config_a;
    config_c.random_seed = 99999;
    const auto res_c = generate_batch_workload(config_c);
    PERF_ASSERT(res_c.is_ok(), "Workload C generation succeeded");
    const auto& vec_c = res_c.value();
    PERF_ASSERT(vec_a[0].length != vec_c[0].length || vec_a[0].left_temperature != vec_c[0].left_temperature,
                "Different random seeds produce different parameter workloads");
}

// 2. Valid generated thermal parameters
void test_valid_generated_parameters() {
    std::cout << "[TEST GROUP 2] Valid Generated Thermal Parameters...\n";

    WorkloadConfig config;
    config.random_seed = 777;
    config.case_count = 30;
    config.elements_per_case = 40;

    const auto gen_res = generate_batch_workload(config);
    PERF_ASSERT(gen_res.is_ok(), "Batch generation succeeds");

    const auto& workload = gen_res.value();
    bool all_valid = true;
    for (const auto& p : workload) {
        if (p.length <= 0.0 || !std::isfinite(p.length) ||
            p.area <= 0.0 || !std::isfinite(p.area) ||
            p.thermal_conductivity <= 0.0 || !std::isfinite(p.thermal_conductivity) ||
            p.heat_generation < 0.0 || !std::isfinite(p.heat_generation) ||
            !std::isfinite(p.left_temperature) || !std::isfinite(p.right_temperature) ||
            p.element_count != 40) {
            all_valid = false;
            break;
        }
    }
    PERF_ASSERT(all_valid, "All generated cases contain strictly valid and finite physical parameters");
}

// 3. Serial batch completion
void test_serial_batch_completion() {
    std::cout << "[TEST GROUP 3] Serial Batch Completion...\n";

    WorkloadConfig config;
    config.case_count = 20;
    config.elements_per_case = 30;
    const auto workload = generate_batch_workload(config).value();

    const auto res = execute_serial_batch(workload);
    PERF_ASSERT(res.mode == ExecutionMode::Serial, "Batch mode is Serial");
    PERF_ASSERT(res.all_succeeded, "All serial cases completed successfully");
    PERF_ASSERT(res.success_count == 20, "Success count matches case count");
    PERF_ASSERT(res.failure_count == 0, "Failure count is exactly 0");
    PERF_ASSERT(res.simulation_results.size() == 20, "Result container size matches case count");
    PERF_ASSERT(res.elapsed_time_ms >= 0.0, "Elapsed time is non-negative");
}

// 4. Parallel batch completion
void test_parallel_batch_completion() {
    std::cout << "[TEST GROUP 4] Parallel Batch Completion Across Worker Counts...\n";

    WorkloadConfig config;
    config.case_count = 20;
    config.elements_per_case = 30;
    const auto workload = generate_batch_workload(config).value();

    for (size_t workers : {1, 2, 3, 4}) {
        const auto res = execute_parallel_batch(workload, workers);
        PERF_ASSERT(res.mode == ExecutionMode::Parallel, "Batch mode is Parallel");
        PERF_ASSERT(res.all_succeeded, "Parallel batch succeeded with requested workers");
        PERF_ASSERT(res.success_count == 20, "Success count is 20 for parallel batch");
        PERF_ASSERT(res.simulation_results.size() == 20, "Result size is 20 for parallel batch");
        PERF_ASSERT(res.actual_workers >= 1, "Actual workers is at least 1");
    }
}

// 5. Correct output ordering
void test_output_ordering() {
    std::cout << "[TEST GROUP 5] Deterministic Output Ordering...\n";

    WorkloadConfig config;
    config.case_count = 25;
    config.elements_per_case = 20;
    const auto workload = generate_batch_workload(config).value();

    const auto par_res = execute_parallel_batch(workload, 4);
    PERF_ASSERT(par_res.all_succeeded, "Parallel batch executed successfully");

    bool ordering_preserved = true;
    for (size_t i = 0; i < workload.size(); ++i) {
        const auto& sol = par_res.simulation_results[i].value();
        // Node 0 should match prescribed temp_left and Node N should match temp_right
        const double t0 = sol.nodes.front().temperature_FE_C;
        const double tL = sol.nodes.back().temperature_FE_C;

        if (std::abs(t0 - workload[i].left_temperature) > 1e-10 ||
            std::abs(tL - workload[i].right_temperature) > 1e-10) {
            ordering_preserved = false;
            break;
        }
    }
    PERF_ASSERT(ordering_preserved, "Output results strictly match input case parameters by index");
}

// 6. Serial/parallel numerical agreement within 1e-8 °C
void test_numerical_agreement() {
    std::cout << "[TEST GROUP 6] Serial/Parallel Agreement Within 1e-8 C...\n";

    WorkloadConfig config;
    config.case_count = 25;
    config.elements_per_case = 50;
    const auto workload = generate_batch_workload(config).value();

    const auto ser_res = execute_serial_batch(workload);
    const auto par_res = execute_parallel_batch(workload, 4);

    const auto verif = verify_batch_results(ser_res, par_res, 1.0e-8);
    PERF_ASSERT(verif.passed, "verify_batch_results passes within 1e-8 C tolerance");
    PERF_ASSERT(verif.max_temperature_difference_C <= 1.0e-8,
                "Maximum absolute temperature difference <= 1e-8 C");
    PERF_ASSERT(verif.compared_cases == 25, "Compared 25 complete simulation cases");
    PERF_ASSERT(verif.compared_nodes == 25 * 51, "Compared all (N+1) nodes across all cases");
}

// 7. Worker count validation and CPU-count capping
void test_worker_count_resolution() {
    std::cout << "[TEST GROUP 7] Worker Count Validation & CPU Capping...\n";

    const unsigned int hw = std::thread::hardware_concurrency();
    const size_t max_expected = (hw == 0) ? 1 : static_cast<size_t>(hw);

    PERF_ASSERT(resolve_worker_count(0) == 1, "Requested 0 workers resolves safely to 1");
    PERF_ASSERT(resolve_worker_count(99999) == max_expected, "Requested 99999 workers capped at logical CPU count");
    if (max_expected >= 2) {
        PERF_ASSERT(resolve_worker_count(2) == 2, "Requested 2 workers resolved accurately");
    }
}

// 8. Empty workload handling
void test_empty_workload() {
    std::cout << "[TEST GROUP 8] Empty Workload Handling...\n";

    const std::vector<thermal::Thermal1DParameters> empty_workload;

    const auto ser_res = execute_serial_batch(empty_workload);
    PERF_ASSERT(ser_res.simulation_results.empty(), "Serial empty workload returns empty results");
    PERF_ASSERT(ser_res.all_succeeded, "Serial empty workload reports success without failure");

    const auto par_res = execute_parallel_batch(empty_workload, 4);
    PERF_ASSERT(par_res.simulation_results.empty(), "Parallel empty workload returns empty results");
    PERF_ASSERT(par_res.all_succeeded, "Parallel empty workload reports success without failure");

    const auto verif = verify_batch_results(ser_res, par_res);
    PERF_ASSERT(verif.passed, "Verifying two empty workloads passes cleanly");
}

// 9. Invalid workload configuration rejection
void test_invalid_workload_rejection() {
    std::cout << "[TEST GROUP 9] Invalid Workload Configuration Rejection...\n";

    WorkloadConfig bad_cases;
    bad_cases.case_count = 0;
    PERF_ASSERT(generate_batch_workload(bad_cases).is_error(), "Zero case count is rejected");

    WorkloadConfig bad_elements;
    bad_elements.elements_per_case = 0;
    PERF_ASSERT(generate_batch_workload(bad_elements).is_error(), "Zero element count is rejected");

    WorkloadConfig bad_length;
    bad_length.length_min = -1.0;
    PERF_ASSERT(generate_batch_workload(bad_length).is_error(), "Negative length range is rejected");
}

// 10. Thread exception and failure handling
void test_failure_handling() {
    std::cout << "[TEST GROUP 10] Worker Failure & Error Isolation...\n";

    WorkloadConfig config;
    config.case_count = 5;
    config.elements_per_case = 20;
    auto workload = generate_batch_workload(config).value();

    // Inject an invalid case in the middle (index 2)
    workload[2].length = -5.0; // Negative length will cause solver to fail

    const auto ser_res = execute_serial_batch(workload);
    PERF_ASSERT(!ser_res.all_succeeded, "Serial batch reports failure when one case is invalid");
    PERF_ASSERT(ser_res.success_count == 4, "Serial batch has 4 successes");
    PERF_ASSERT(ser_res.failure_count == 1, "Serial batch has 1 failure");
    PERF_ASSERT(ser_res.simulation_results[2].is_error(), "Serial result at index 2 is an error");

    const auto par_res = execute_parallel_batch(workload, 2);
    PERF_ASSERT(!par_res.all_succeeded, "Parallel batch reports failure when one case is invalid");
    PERF_ASSERT(par_res.success_count == 4, "Parallel batch has 4 successes");
    PERF_ASSERT(par_res.failure_count == 1, "Parallel batch has 1 failure");
    PERF_ASSERT(par_res.simulation_results[2].is_error(), "Parallel result at index 2 is an error");

    // Correctness verifier should confirm consistent failure reporting
    const auto verif = verify_batch_results(ser_res, par_res);
    PERF_ASSERT(verif.passed, "Both modes consistently report failure at index 2 without divergence");
}

// 11. Benchmark median calculation
void test_median_calculation() {
    std::cout << "[TEST GROUP 11] Median Calculation Accuracy...\n";

    PERF_ASSERT(calculate_median({}) == 0.0, "Empty samples vector returns 0.0");
    PERF_ASSERT(calculate_median({42.5}) == 42.5, "Single sample returns value directly");

    // Odd count: 5 elements
    std::vector<double> odd_samples = {50.0, 10.0, 40.0, 20.0, 30.0};
    PERF_ASSERT(calculate_median(odd_samples) == 30.0, "Odd sample median is exactly middle element");

    // Even count: 4 elements
    std::vector<double> even_samples = {40.0, 10.0, 30.0, 20.0};
    PERF_ASSERT(calculate_median(even_samples) == 25.0, "Even sample median is average of two middle elements");
}

// 12. Speedup and efficiency calculations
void test_speedup_and_efficiency() {
    std::cout << "[TEST GROUP 12] Speedup and Efficiency Formulations...\n";

    const double t_serial = 100.0;
    const double t_parallel = 50.0;
    const size_t workers = 2;

    const double speedup = t_serial / t_parallel;
    const double eff_ratio = speedup / static_cast<double>(workers);
    const double eff_pct = eff_ratio * 100.0;

    PERF_ASSERT(std::abs(speedup - 2.0) < 1e-12, "Speedup matches 2.0x");
    PERF_ASSERT(std::abs(eff_ratio - 1.0) < 1e-12, "Efficiency ratio is 1.0");
    PERF_ASSERT(std::abs(eff_pct - 100.0) < 1e-12, "Efficiency percentage is 100.0%");

    // With 4 workers and t_parallel = 40.0 -> speedup = 2.5x -> efficiency = 62.5%
    const double speedup_4 = t_serial / 40.0;
    const double eff_pct_4 = (speedup_4 / 4.0) * 100.0;
    PERF_ASSERT(std::abs(speedup_4 - 2.5) < 1e-12, "Speedup with 4 workers matches 2.5x");
    PERF_ASSERT(std::abs(eff_pct_4 - 62.5) < 1e-12, "Efficiency percentage with 4 workers is 62.5%");
}

// 13. CSV export headers and file output
void test_csv_export() {
    std::cout << "[TEST GROUP 13] Raw and Summary CSV Exporters...\n";

    WorkloadConfig config;
    config.case_count = 5;
    config.elements_per_case = 20;

    // Run small benchmark suite with 1 and 2 workers
    const auto suite_res = run_benchmark_suite(config, {1, 2});
    PERF_ASSERT(suite_res.is_ok(), "Benchmark suite runs successfully for CSV test");
    const auto& suite = suite_res.value();

    const std::string raw_csv = "test_perf_raw.csv";
    const std::string sum_csv = "test_perf_sum.csv";

    // Export raw CSV
    const auto raw_status = export_raw_benchmark_csv(raw_csv, suite);
    PERF_ASSERT(raw_status.is_ok(), "export_raw_benchmark_csv succeeds");

    std::ifstream in_raw(raw_csv);
    PERF_ASSERT(in_raw.is_open(), "Raw CSV is openable");
    std::string raw_header;
    std::getline(in_raw, raw_header);
    PERF_ASSERT(raw_header == "execution_mode,worker_count,repetition,elapsed_time_ms,case_count,elements_per_case,random_seed,max_temperature_difference_C,correctness_passed",
                "Raw CSV header matches required specification");
    in_raw.close();
    std::remove(raw_csv.c_str());

    // Export summary CSV
    const auto sum_status = export_summary_benchmark_csv(sum_csv, suite);
    PERF_ASSERT(sum_status.is_ok(), "export_summary_benchmark_csv succeeds");

    std::ifstream in_sum(sum_csv);
    PERF_ASSERT(in_sum.is_open(), "Summary CSV is openable");
    std::string sum_header;
    std::getline(in_sum, sum_header);
    PERF_ASSERT(sum_header == "execution_mode,worker_count,median_time_ms,speedup,efficiency,correctness_status",
                "Summary CSV header matches required specification");
    in_sum.close();
    std::remove(sum_csv.c_str());

    // Test invalid filepath error handling
    const auto bad_export = export_raw_benchmark_csv("Z:/invalid_dir/bad.csv", suite);
    PERF_ASSERT(bad_export.is_error(), "Export to invalid path returns error status");
}

// 14. Detection of non-finite temperatures and missing results
void test_non_finite_rejection() {
    std::cout << "[TEST GROUP 14] Detection of Non-Finite and Missing Results...\n";

    WorkloadConfig config;
    config.case_count = 5;
    config.elements_per_case = 20;
    const auto workload = generate_batch_workload(config).value();

    auto ser_res = execute_serial_batch(workload);
    auto par_res = execute_parallel_batch(workload, 2);

    // Artificially inject NaN into parallel result
    par_res.simulation_results[1].value().nodes[5].temperature_FE_C = std::numeric_limits<double>::quiet_NaN();

    const auto verif_nan = verify_batch_results(ser_res, par_res);
    PERF_ASSERT(!verif_nan.passed, "verify_batch_results rejects non-finite NaN temperature");

    // Artificially change result size (simulate missing result)
    par_res.simulation_results.pop_back();
    const auto verif_size = verify_batch_results(ser_res, par_res);
    PERF_ASSERT(!verif_size.passed, "verify_batch_results rejects case count mismatch / missing results");
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  UNIFIED ENGINEERING WORKBENCH TESTS  \n";
    std::cout << "  Stage 6: Multicore Benchmarking (Task D)\n";
    std::cout << "========================================\n\n";

    test_fixed_seed_reproducibility();
    test_valid_generated_parameters();
    test_serial_batch_completion();
    test_parallel_batch_completion();
    test_output_ordering();
    test_numerical_agreement();
    test_worker_count_resolution();
    test_empty_workload();
    test_invalid_workload_rejection();
    test_failure_handling();
    test_median_calculation();
    test_speedup_and_efficiency();
    test_csv_export();
    test_non_finite_rejection();

    std::cout << "\n========================================\n";
    std::cout << "  PERFORMANCE TESTS SUMMARY: " << g_perf_passed << " PASSED, " << g_perf_failed << " FAILED\n";
    std::cout << "========================================\n";

    return (g_perf_failed == 0) ? 0 : 1;
}
