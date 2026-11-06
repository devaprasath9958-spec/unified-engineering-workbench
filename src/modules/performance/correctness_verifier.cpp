#include "modules/performance/correctness_verifier.h"

#include <cmath>
#include <sstream>
#include <algorithm>

namespace workbench::performance {

VerificationResult verify_batch_results(
    const BatchRunResult& serial,
    const BatchRunResult& parallel,
    double tolerance_C
) {
    VerificationResult result;
    result.compared_cases = serial.simulation_results.size();

    // 1. Verify case counts match
    if (serial.simulation_results.size() != parallel.simulation_results.size()) {
        result.passed = false;
        std::ostringstream oss;
        oss << "Simulation case count mismatch: serial produced " << serial.simulation_results.size()
            << " results but parallel produced " << parallel.simulation_results.size() << ".";
        result.error_message = oss.str();
        return result;
    }

    double max_diff = 0.0;
    size_t total_nodes = 0;

    // 2. Iterate across every case by index
    for (size_t c = 0; c < serial.simulation_results.size(); ++c) {
        const auto& ser_res = serial.simulation_results[c];
        const auto& par_res = parallel.simulation_results[c];

        // Status consistency
        if (ser_res.is_ok() != par_res.is_ok()) {
            result.passed = false;
            std::ostringstream oss;
            oss << "Status mismatch at case index " << c << ": serial is "
                << (ser_res.is_ok() ? "OK" : "FAILURE") << ", parallel is "
                << (par_res.is_ok() ? "OK" : "FAILURE") << ".";
            result.error_message = oss.str();
            return result;
        }

        // If both failed, they agree on failure
        if (!ser_res.is_ok()) {
            continue;
        }

        const auto& ser_sol = ser_res.value();
        const auto& par_sol = par_res.value();

        // Mesh size consistency
        if (ser_sol.nodes.size() != par_sol.nodes.size()) {
            result.passed = false;
            std::ostringstream oss;
            oss << "Node count mismatch at case index " << c << ": serial has "
                << ser_sol.nodes.size() << ", parallel has " << par_sol.nodes.size() << ".";
            result.error_message = oss.str();
            return result;
        }

        // Compare individual node temperatures
        for (size_t n = 0; n < ser_sol.nodes.size(); ++n) {
            const double t_ser = ser_sol.nodes[n].temperature_FE_C;
            const double t_par = par_sol.nodes[n].temperature_FE_C;

            if (!std::isfinite(t_ser) || !std::isfinite(t_par)) {
                result.passed = false;
                std::ostringstream oss;
                oss << "Non-finite temperature detected at case " << c << ", node " << n
                    << ": serial=" << t_ser << ", parallel=" << t_par << ".";
                result.error_message = oss.str();
                return result;
            }

            const double diff = std::abs(t_ser - t_par);
            if (diff > max_diff) {
                max_diff = diff;
            }

            if (diff > tolerance_C) {
                result.passed = false;
                result.max_temperature_difference_C = max_diff;
                std::ostringstream oss;
                oss << "Temperature difference exceeded tolerance at case " << c << ", node " << n
                    << ": diff=" << diff << " C, tolerance=" << tolerance_C << " C.";
                result.error_message = oss.str();
                return result;
            }
            total_nodes++;
        }
    }

    result.passed = true;
    result.max_temperature_difference_C = max_diff;
    result.compared_nodes = total_nodes;
    return result;
}

} // namespace workbench::performance
