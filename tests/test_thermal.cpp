#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <limits>
#include <string>

#include "modules/thermal/fem_1d_solver.h"
#include "core/validation.h"

using namespace workbench::thermal;
using namespace workbench::core;

static int g_thermal_passed = 0;
static int g_thermal_failed = 0;

#define THERMAL_ASSERT(expr, msg) \
    do { \
        if (expr) { \
            g_thermal_passed++; \
        } else { \
            g_thermal_failed++; \
            std::cerr << "  [FAIL] " << msg << " (" << #expr << ") at line " << __LINE__ << "\n"; \
        } \
    } while (0)

void test_analytical_solution() {
    std::cout << "[TEST GROUP] Analytical Exact Temperature Verification...\n";

    Thermal1DParameters params;
    params.length = 1.0;
    params.area = 1.0e-4;
    params.thermal_conductivity = 10.0;
    params.heat_generation = 10000.0;
    params.left_temperature = 100.0;
    params.right_temperature = 20.0;

    // At x = 0: T = 100
    THERMAL_ASSERT(validation::approx_equal(compute_exact_temperature(0.0, params), 100.0, 1e-9),
                   "T_exact(0) is exactly 100 C");

    // At x = 1: T = 20
    THERMAL_ASSERT(validation::approx_equal(compute_exact_temperature(1.0, params), 20.0, 1e-9),
                   "T_exact(1) is exactly 20 C");

    // At x = 0.5: T = 100 + 420(0.5) - 500(0.25) = 100 + 210 - 125 = 185 C
    THERMAL_ASSERT(validation::approx_equal(compute_exact_temperature(0.5, params), 185.0, 1e-9),
                   "T_exact(0.5) is exactly 185 C");
}

void test_mesh_and_boundaries() {
    std::cout << "[TEST GROUP] 1D Mesh Generation and Boundary Condition Enforcement...\n";

    Thermal1DParameters params;
    params.element_count = 10;

    const auto res = solve_steady_state_1d(params);
    THERMAL_ASSERT(res.is_ok(), "Solving N=10 succeeds");

    if (res.is_ok()) {
        const auto& sol = res.value();
        THERMAL_ASSERT(sol.nodes.size() == 11, "N=10 produces 11 nodes");
        THERMAL_ASSERT(sol.elements.size() == 10, "N=10 produces 10 elements");
        THERMAL_ASSERT(validation::approx_equal(sol.element_length, 0.1, 1e-9), "Element length h is 0.1 m");

        // Boundary conditions
        THERMAL_ASSERT(validation::approx_equal(sol.nodes.front().temperature_FE_C, 100.0, 1e-9),
                       "Node 0 temperature is exactly 100.0 C");
        THERMAL_ASSERT(validation::approx_equal(sol.nodes.back().temperature_FE_C, 20.0, 1e-9),
                       "Node 10 temperature is exactly 20.0 C");
    }
}

void test_nodal_exact_agreement() {
    std::cout << "[TEST GROUP] Nodal Temperature Agreement with Analytical Solution...\n";

    Thermal1DParameters params;
    params.element_count = 10;

    const auto res = solve_steady_state_1d(params);
    THERMAL_ASSERT(res.is_ok(), "Solving N=10 succeeds");

    if (res.is_ok()) {
        const auto& sol = res.value();
        double max_nodal_err = 0.0;
        for (const auto& node : sol.nodes) {
            max_nodal_err = std::max(max_nodal_err, node.absolute_error_C);
        }
        // For linear elements with uniform heat generation, nodal values match analytical exactly
        THERMAL_ASSERT(max_nodal_err < 1.0e-11, "Nodal temperatures match exact analytical solution to within 1e-11 C");
    }
}

void test_convergence_n10_20_40_80() {
    std::cout << "[TEST GROUP] Midpoint Convergence Analysis for N = 10, 20, 40, 80...\n";

    Thermal1DParameters params;

    // 1. N = 10
    params.element_count = 10;
    auto res_10 = solve_steady_state_1d(params);
    THERMAL_ASSERT(res_10.is_ok(), "N=10 solve succeeds");
    const double err_10 = res_10.value().max_absolute_midpoint_error_C;
    THERMAL_ASSERT(validation::approx_equal(err_10, 1.25, 1e-6),
                   "N=10 maximum midpoint error is 1.25 C");

    // 2. N = 20
    params.element_count = 20;
    auto res_20 = solve_steady_state_1d(params);
    THERMAL_ASSERT(res_20.is_ok(), "N=20 solve succeeds");
    const double err_20 = res_20.value().max_absolute_midpoint_error_C;
    THERMAL_ASSERT(validation::approx_equal(err_20, 0.3125, 1e-6),
                   "N=20 maximum midpoint error is 0.3125 C");

    // 3. N = 40
    params.element_count = 40;
    auto res_40 = solve_steady_state_1d(params);
    THERMAL_ASSERT(res_40.is_ok(), "N=40 solve succeeds");
    const double err_40 = res_40.value().max_absolute_midpoint_error_C;
    THERMAL_ASSERT(validation::approx_equal(err_40, 0.078125, 1e-6),
                   "N=40 maximum midpoint error is 0.078125 C");

    // 4. N = 80
    params.element_count = 80;
    auto res_80 = solve_steady_state_1d(params);
    THERMAL_ASSERT(res_80.is_ok(), "N=80 solve succeeds");
    const double err_80 = res_80.value().max_absolute_midpoint_error_C;
    THERMAL_ASSERT(validation::approx_equal(err_80, 0.01953125, 1e-6),
                   "N=80 maximum midpoint error is 0.01953125 C");

    // 5. Convergence rate verification (O(h^2) reduction by 4.0x on doubling N)
    const double ratio_10_20 = err_10 / err_20;
    const double ratio_20_40 = err_20 / err_40;
    const double ratio_40_80 = err_40 / err_80;

    THERMAL_ASSERT(validation::approx_equal(ratio_10_20, 4.0, 1e-5), "Error reduction ratio N=10 -> N=20 is 4.0x");
    THERMAL_ASSERT(validation::approx_equal(ratio_20_40, 4.0, 1e-5), "Error reduction ratio N=20 -> N=40 is 4.0x");
    THERMAL_ASSERT(validation::approx_equal(ratio_40_80, 4.0, 1e-5), "Error reduction ratio N=40 -> N=80 is 4.0x");
}

void test_invalid_parameters() {
    std::cout << "[TEST GROUP] Input Validation & Invalid Parameter Rejection...\n";

    Thermal1DParameters params;

    // Length <= 0
    params.length = 0.0;
    auto res_len0 = solve_steady_state_1d(params);
    THERMAL_ASSERT(res_len0.is_error(), "Zero length rejected");
    params.length = -1.0;
    auto res_neg_len = solve_steady_state_1d(params);
    THERMAL_ASSERT(res_neg_len.is_error(), "Negative length rejected");
    params.length = 1.0;

    // Area <= 0
    params.area = 0.0;
    auto res_area0 = solve_steady_state_1d(params);
    THERMAL_ASSERT(res_area0.is_error(), "Zero area rejected");
    params.area = 1e-4;

    // Conductivity <= 0
    params.thermal_conductivity = -5.0;
    auto res_neg_k = solve_steady_state_1d(params);
    THERMAL_ASSERT(res_neg_k.is_error(), "Negative conductivity rejected");
    params.thermal_conductivity = 10.0;

    // Element count == 0
    params.element_count = 0;
    auto res_elem0 = solve_steady_state_1d(params);
    THERMAL_ASSERT(res_elem0.is_error(), "Zero element count rejected");
    params.element_count = 10;
}

void test_non_finite_inputs() {
    std::cout << "[TEST GROUP] Non-Finite Input Handling (NaN / Inf)...\n";

    Thermal1DParameters params;

    // NaN heat generation
    params.heat_generation = std::numeric_limits<double>::quiet_NaN();
    THERMAL_ASSERT(solve_steady_state_1d(params).is_error(), "NaN heat generation rejected");
    params.heat_generation = 10000.0;

    // Inf boundary temperature
    params.left_temperature = std::numeric_limits<double>::infinity();
    THERMAL_ASSERT(solve_steady_state_1d(params).is_error(), "Inf left temperature rejected");
    params.left_temperature = 100.0;

    // NaN length
    params.length = std::numeric_limits<double>::quiet_NaN();
    THERMAL_ASSERT(solve_steady_state_1d(params).is_error(), "NaN length rejected");
    params.length = 1.0;
}

void test_csv_export() {
    std::cout << "[TEST GROUP] CSV Export & File I/O Verification...\n";

    Thermal1DParameters params;
    params.element_count = 10;
    const auto sol_res = solve_steady_state_1d(params);
    THERMAL_ASSERT(sol_res.is_ok(), "Solve for CSV export succeeds");

    const std::string test_csv_path = "results_thermal_test.csv";
    const auto export_status = export_fem_results_to_csv(test_csv_path, sol_res.value());
    THERMAL_ASSERT(export_status.is_ok(), "export_fem_results_to_csv succeeds");

    // Read back and inspect lines
    std::ifstream in(test_csv_path);
    THERMAL_ASSERT(in.is_open(), "Exported CSV file is openable");

    std::string header;
    std::getline(in, header);
    THERMAL_ASSERT(header == "x_m,temperature_FE_C,temperature_exact_C,absolute_error_C",
                   "CSV header matches required column specification");

    size_t line_count = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            line_count++;
        }
    }
    // Combined 11 nodes + 10 midpoints = 21 evaluation rows
    THERMAL_ASSERT(line_count == 21, "CSV contains exactly 21 evaluation rows (nodes + midpoints)");
    in.close();

    // Clean up temporary test file
    std::remove(test_csv_path.c_str());

    // Test failure handling on invalid file path
    const std::string invalid_path = "Z:/non_existent_drive/invalid_file.csv";
    const auto bad_export = export_fem_results_to_csv(invalid_path, sol_res.value());
    THERMAL_ASSERT(bad_export.is_error(), "Export to invalid path returns error");
    THERMAL_ASSERT(!bad_export.error().message.empty(), "Error message is non-empty and actionable");
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  UNIFIED ENGINEERING WORKBENCH TESTS  \n";
    std::cout << "  Stage 5: 1D Steady-State Thermal FEM \n";
    std::cout << "========================================\n\n";

    test_analytical_solution();
    test_mesh_and_boundaries();
    test_nodal_exact_agreement();
    test_convergence_n10_20_40_80();
    test_invalid_parameters();
    test_non_finite_inputs();
    test_csv_export();

    std::cout << "\n========================================\n";
    std::cout << "  THERMAL FEM TESTS SUMMARY: " << g_thermal_passed << " PASSED, " << g_thermal_failed << " FAILED\n";
    std::cout << "========================================\n";

    return (g_thermal_failed == 0) ? 0 : 1;
}
