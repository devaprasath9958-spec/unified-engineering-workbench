#pragma once

#include "core/result.h"
#include "modules/thermal/fem_1d_solver.h"

#include <vector>
#include <cstdint>
#include <cstddef>

namespace workbench::performance {

/// @brief Configuration settings for generating reproducible batches of thermal simulation cases.
struct WorkloadConfig {
    uint32_t random_seed = 42;
    size_t case_count = 100;
    size_t elements_per_case = 80;

    // Realistic bounds for valid physical parameters
    double length_min = 0.5;
    double length_max = 2.0;

    double area_min = 5.0e-5;
    double area_max = 5.0e-4;

    double conductivity_min = 5.0;
    double conductivity_max = 50.0;

    double heat_gen_min = 1000.0;
    double heat_gen_max = 20000.0;

    double temp_left_min = 50.0;
    double temp_left_max = 150.0;

    double temp_right_min = 10.0;
    double temp_right_max = 40.0;
};

/// @brief Generates a deterministic batch of independent, numerically valid 1D thermal simulation cases.
/// @param config Workload settings including random seed, case count, and mesh size.
/// @return Result containing the vector of generated cases or an error status if parameters are invalid.
core::Result<std::vector<thermal::Thermal1DParameters>> generate_batch_workload(const WorkloadConfig& config);

} // namespace workbench::performance
