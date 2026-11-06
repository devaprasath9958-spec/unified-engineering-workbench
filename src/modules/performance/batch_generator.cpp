#include "modules/performance/batch_generator.h"

#include <random>
#include <cmath>

namespace workbench::performance {

core::Result<std::vector<thermal::Thermal1DParameters>> generate_batch_workload(const WorkloadConfig& config) {
    if (config.case_count == 0) {
        return core::Result<std::vector<thermal::Thermal1DParameters>>::failure(
            core::StatusCode::InvalidInput,
            "Cannot generate batch workload: case_count must be at least 1."
        );
    }

    if (config.elements_per_case == 0) {
        return core::Result<std::vector<thermal::Thermal1DParameters>>::failure(
            core::StatusCode::InvalidInput,
            "Cannot generate batch workload: elements_per_case must be at least 1."
        );
    }

    if (config.length_min <= 0.0 || config.length_max < config.length_min ||
        !std::isfinite(config.length_min) || !std::isfinite(config.length_max)) {
        return core::Result<std::vector<thermal::Thermal1DParameters>>::failure(
            core::StatusCode::InvalidInput,
            "Invalid length range for batch workload generator."
        );
    }

    if (config.area_min <= 0.0 || config.area_max < config.area_min ||
        !std::isfinite(config.area_min) || !std::isfinite(config.area_max)) {
        return core::Result<std::vector<thermal::Thermal1DParameters>>::failure(
            core::StatusCode::InvalidInput,
            "Invalid cross-sectional area range for batch workload generator."
        );
    }

    if (config.conductivity_min <= 0.0 || config.conductivity_max < config.conductivity_min ||
        !std::isfinite(config.conductivity_min) || !std::isfinite(config.conductivity_max)) {
        return core::Result<std::vector<thermal::Thermal1DParameters>>::failure(
            core::StatusCode::InvalidInput,
            "Invalid thermal conductivity range for batch workload generator."
        );
    }

    if (config.heat_gen_min < 0.0 || config.heat_gen_max < config.heat_gen_min ||
        !std::isfinite(config.heat_gen_min) || !std::isfinite(config.heat_gen_max)) {
        return core::Result<std::vector<thermal::Thermal1DParameters>>::failure(
            core::StatusCode::InvalidInput,
            "Invalid heat generation range for batch workload generator."
        );
    }

    if (!std::isfinite(config.temp_left_min) || !std::isfinite(config.temp_left_max) ||
        !std::isfinite(config.temp_right_min) || !std::isfinite(config.temp_right_max)) {
        return core::Result<std::vector<thermal::Thermal1DParameters>>::failure(
            core::StatusCode::InvalidInput,
            "Temperature bounds must be finite for batch workload generator."
        );
    }

    std::vector<thermal::Thermal1DParameters> workload;
    workload.reserve(config.case_count);

    // Fixed-seed deterministic Mersenne Twister engine
    std::mt19937_64 rng(config.random_seed);

    std::uniform_real_distribution<double> dist_len(config.length_min, config.length_max);
    std::uniform_real_distribution<double> dist_area(config.area_min, config.area_max);
    std::uniform_real_distribution<double> dist_cond(config.conductivity_min, config.conductivity_max);
    std::uniform_real_distribution<double> dist_q(config.heat_gen_min, config.heat_gen_max);
    std::uniform_real_distribution<double> dist_t0(config.temp_left_min, config.temp_left_max);
    std::uniform_real_distribution<double> dist_tL(config.temp_right_min, config.temp_right_max);

    for (size_t i = 0; i < config.case_count; ++i) {
        thermal::Thermal1DParameters params;
        params.element_count = config.elements_per_case;
        params.length = dist_len(rng);
        params.area = dist_area(rng);
        params.thermal_conductivity = dist_cond(rng);
        params.heat_generation = dist_q(rng);
        params.left_temperature = dist_t0(rng);
        params.right_temperature = dist_tL(rng);

        workload.push_back(params);
    }

    return core::Result<std::vector<thermal::Thermal1DParameters>>::success(std::move(workload));
}

} // namespace workbench::performance
