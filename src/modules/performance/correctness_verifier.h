#pragma once

#include "modules/performance/batch_runner.h"

#include <string>

namespace workbench::performance {

/// @brief Holds the evaluation results of comparing parallel execution against serial reference.
struct VerificationResult {
    bool passed = false;
    double max_temperature_difference_C = 0.0;
    size_t compared_cases = 0;
    size_t compared_nodes = 0;
    std::string error_message;
};

/// @brief Verifies that parallel simulation results match the serial reference within tolerance.
/// @param serial Serial batch reference run.
/// @param parallel Parallel batch execution run.
/// @param tolerance_C Maximum allowed absolute temperature difference in °C (default 1e-8).
/// @return VerificationResult indicating pass/fail status and the maximum observed difference.
VerificationResult verify_batch_results(
    const BatchRunResult& serial,
    const BatchRunResult& parallel,
    double tolerance_C = 1.0e-8
);

} // namespace workbench::performance
