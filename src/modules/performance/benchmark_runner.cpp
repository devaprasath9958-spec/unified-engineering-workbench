#include "modules/performance/benchmark_runner.h"

#include <fstream>
#include <iomanip>
#include <algorithm>
#include <sstream>

namespace workbench::performance {

double calculate_median(std::vector<double> samples) {
    if (samples.empty()) {
        return 0.0;
    }
    std::sort(samples.begin(), samples.end());
    const size_t n = samples.size();
    if (n % 2 == 1) {
        return samples[n / 2];
    }
    return 0.5 * (samples[(n / 2) - 1] + samples[n / 2]);
}

core::Result<BenchmarkSuiteResult> run_benchmark_suite(
    const WorkloadConfig& config,
    const std::vector<size_t>& worker_targets
) {
    // 1. Generate workload before timing
    auto gen_res = generate_batch_workload(config);
    if (!gen_res.is_ok()) {
        return core::Result<BenchmarkSuiteResult>::failure(
            gen_res.error().code,
            "Benchmark failed during workload generation: " + gen_res.error().message
        );
    }
    const auto& workload = gen_res.value();

    BenchmarkSuiteResult suite;
    suite.system_metadata = get_system_metadata();
    suite.workload_config = config;
    suite.warmup_count = 1;
    suite.measured_repetitions = 5;

    // 2. Pre-generate serial reference and verify it succeeds
    // Warm-up run for serial
    execute_serial_batch(workload);

    const auto serial_ref = execute_serial_batch(workload);
    if (!serial_ref.all_succeeded) {
        return core::Result<BenchmarkSuiteResult>::failure(
            core::StatusCode::ComputationError,
            "Serial reference simulation failed. Cannot conduct benchmark without valid reference."
        );
    }

    // 3. Benchmark Serial Mode (5 measured repetitions)
    std::vector<double> serial_times;
    serial_times.reserve(suite.measured_repetitions);

    for (size_t rep = 0; rep < suite.measured_repetitions; ++rep) {
        const auto run = execute_serial_batch(workload);
        serial_times.push_back(run.elapsed_time_ms);

        BenchmarkRepetitionRecord rec;
        rec.execution_mode = "Serial";
        rec.requested_workers = 1;
        rec.actual_workers = 1;
        rec.repetition_index = rep + 1;
        rec.elapsed_time_ms = run.elapsed_time_ms;
        rec.case_count = config.case_count;
        rec.elements_per_case = config.elements_per_case;
        rec.random_seed = config.random_seed;
        rec.max_temperature_difference_C = 0.0;
        rec.correctness_passed = run.all_succeeded;

        suite.raw_records.push_back(rec);
    }

    const double serial_median_ms = calculate_median(serial_times);

    BenchmarkModeSummary serial_summary;
    serial_summary.execution_mode = "Serial";
    serial_summary.requested_workers = 1;
    serial_summary.actual_workers = 1;
    serial_summary.median_time_ms = serial_median_ms;
    serial_summary.speedup = 1.0;
    serial_summary.efficiency_ratio = 1.0;
    serial_summary.efficiency_percent = 100.0;
    serial_summary.all_repetitions_correct = true;
    serial_summary.was_tested = true;
    serial_summary.note = "Baseline serial reference";

    suite.summaries.push_back(serial_summary);

    bool all_modes_correct = true;

    // 4. Benchmark Parallel Worker Targets
    for (size_t req_w : worker_targets) {
        const size_t actual_w = resolve_worker_count(req_w);

        // Check if machine has enough logical processors to test this exact requested worker count
        if (req_w > suite.system_metadata.logical_processor_count) {
            BenchmarkModeSummary skipped_summary;
            std::ostringstream mode_name;
            mode_name << "Parallel (" << req_w << " workers requested)";
            skipped_summary.execution_mode = mode_name.str();
            skipped_summary.requested_workers = req_w;
            skipped_summary.actual_workers = actual_w;
            skipped_summary.median_time_ms = 0.0;
            skipped_summary.speedup = 0.0;
            skipped_summary.efficiency_ratio = 0.0;
            skipped_summary.efficiency_percent = 0.0;
            skipped_summary.all_repetitions_correct = false;
            skipped_summary.was_tested = false;
            std::ostringstream note_ss;
            note_ss << "Cannot test " << req_w << " workers: exceeds host logical processors ("
                    << suite.system_metadata.logical_processor_count << "). Capped to " << actual_w << ".";
            skipped_summary.note = note_ss.str();
            suite.summaries.push_back(skipped_summary);
            continue;
        }

        // 1 warm-up run (excluded from measured repetitions)
        execute_parallel_batch(workload, actual_w);

        std::vector<double> par_times;
        par_times.reserve(suite.measured_repetitions);
        bool mode_correct = true;

        for (size_t rep = 0; rep < suite.measured_repetitions; ++rep) {
            const auto run = execute_parallel_batch(workload, actual_w);
            par_times.push_back(run.elapsed_time_ms);

            // Verify against serial reference
            const auto verif = verify_batch_results(serial_ref, run);
            if (!verif.passed) {
                mode_correct = false;
                all_modes_correct = false;
            }

            BenchmarkRepetitionRecord rec;
            std::ostringstream mode_name;
            mode_name << "Parallel (" << actual_w << " workers)";
            rec.execution_mode = mode_name.str();
            rec.requested_workers = req_w;
            rec.actual_workers = actual_w;
            rec.repetition_index = rep + 1;
            rec.elapsed_time_ms = run.elapsed_time_ms;
            rec.case_count = config.case_count;
            rec.elements_per_case = config.elements_per_case;
            rec.random_seed = config.random_seed;
            rec.max_temperature_difference_C = verif.max_temperature_difference_C;
            rec.correctness_passed = verif.passed;

            suite.raw_records.push_back(rec);
        }

        const double par_median_ms = calculate_median(par_times);

        BenchmarkModeSummary par_summary;
        std::ostringstream mode_name;
        mode_name << "Parallel (" << actual_w << " workers)";
        par_summary.execution_mode = mode_name.str();
        par_summary.requested_workers = req_w;
        par_summary.actual_workers = actual_w;
        par_summary.median_time_ms = par_median_ms;
        par_summary.all_repetitions_correct = mode_correct;
        par_summary.was_tested = true;

        if (mode_correct && par_median_ms > 0.0) {
            par_summary.speedup = serial_median_ms / par_median_ms;
            par_summary.efficiency_ratio = par_summary.speedup / static_cast<double>(actual_w);
            par_summary.efficiency_percent = par_summary.efficiency_ratio * 100.0;
            par_summary.note = "Correctness verified (difference <= 1e-8 C)";
        } else {
            par_summary.speedup = 0.0;
            par_summary.efficiency_ratio = 0.0;
            par_summary.efficiency_percent = 0.0;
            par_summary.note = "Correctness verification failed: speedup marked invalid";
        }

        suite.summaries.push_back(par_summary);
    }

    suite.overall_correctness_passed = all_modes_correct;
    return core::Result<BenchmarkSuiteResult>::success(std::move(suite));
}

core::Status export_raw_benchmark_csv(
    const std::string& filepath,
    const BenchmarkSuiteResult& suite
) {
    if (filepath.empty()) {
        return core::Status::failure(
            core::StatusCode::InvalidInput,
            "Cannot export raw benchmark CSV: filepath is empty."
        );
    }

    std::ofstream out(filepath);
    if (!out.is_open()) {
        return core::Status::failure(
            core::StatusCode::FileNotFound,
            "Failed to open or create raw benchmark CSV file: " + filepath
        );
    }

    // Required CSV Header
    out << "execution_mode,worker_count,repetition,elapsed_time_ms,case_count,elements_per_case,random_seed,max_temperature_difference_C,correctness_passed\n";

    out << std::fixed << std::setprecision(6);
    for (const auto& rec : suite.raw_records) {
        out << "\"" << rec.execution_mode << "\","
            << rec.actual_workers << ","
            << rec.repetition_index << ","
            << rec.elapsed_time_ms << ","
            << rec.case_count << ","
            << rec.elements_per_case << ","
            << rec.random_seed << ","
            << std::scientific << std::setprecision(8) << rec.max_temperature_difference_C << ","
            << std::fixed
            << (rec.correctness_passed ? "PASSED" : "FAILED") << "\n";
    }

    if (out.fail()) {
        return core::Status::failure(
            core::StatusCode::ComputationError,
            "Stream write failure occurred while writing raw benchmark CSV: " + filepath
        );
    }

    return core::Status::ok();
}

core::Status export_summary_benchmark_csv(
    const std::string& filepath,
    const BenchmarkSuiteResult& suite
) {
    if (filepath.empty()) {
        return core::Status::failure(
            core::StatusCode::InvalidInput,
            "Cannot export summary benchmark CSV: filepath is empty."
        );
    }

    std::ofstream out(filepath);
    if (!out.is_open()) {
        return core::Status::failure(
            core::StatusCode::FileNotFound,
            "Failed to open or create summary benchmark CSV file: " + filepath
        );
    }

    // Required Summary CSV Header
    out << "execution_mode,worker_count,median_time_ms,speedup,efficiency,correctness_status\n";

    out << std::fixed;
    for (const auto& sum : suite.summaries) {
        if (!sum.was_tested) {
            out << "\"" << sum.execution_mode << "\","
                << sum.actual_workers << ","
                << "N/A,N/A,N/A,"
                << "\"SKIPPED: " << sum.note << "\"\n";
            continue;
        }

        out << "\"" << sum.execution_mode << "\","
            << sum.actual_workers << ","
            << std::setprecision(4) << sum.median_time_ms << ","
            << std::setprecision(4) << sum.speedup << ","
            << std::setprecision(2) << sum.efficiency_percent << "%,"
            << (sum.all_repetitions_correct ? "PASSED" : "FAILED") << "\n";
    }

    if (out.fail()) {
        return core::Status::failure(
            core::StatusCode::ComputationError,
            "Stream write failure occurred while writing summary benchmark CSV: " + filepath
        );
    }

    return core::Status::ok();
}

} // namespace workbench::performance
