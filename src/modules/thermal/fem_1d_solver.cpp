#include "modules/thermal/fem_1d_solver.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <algorithm>

namespace workbench::thermal {

double compute_exact_temperature(double x, const Thermal1DParameters& params) noexcept {
    // T_exact(x) = T(0) + (T(L) - T(0))*x/L + (q / (2*k)) * x * (L - x)
    const double linear_slope = (params.right_temperature - params.left_temperature) / params.length;
    const double heat_term = (params.heat_generation / (2.0 * params.thermal_conductivity)) * x * (params.length - x);
    return params.left_temperature + linear_slope * x + heat_term;
}

core::Result<Fem1DResult> solve_steady_state_1d(const Thermal1DParameters& params) {
    // 1. Parameter Validation
    if (!std::isfinite(params.length) || params.length <= 0.0) {
        return core::Result<Fem1DResult>::failure(
            core::StatusCode::InvalidInput,
            "Rod length must be positive and finite (provided: " + std::to_string(params.length) + ")."
        );
    }

    if (!std::isfinite(params.area) || params.area <= 0.0) {
        return core::Result<Fem1DResult>::failure(
            core::StatusCode::InvalidInput,
            "Cross-sectional area must be positive and finite (provided: " + std::to_string(params.area) + ")."
        );
    }

    if (!std::isfinite(params.thermal_conductivity) || params.thermal_conductivity <= 0.0) {
        return core::Result<Fem1DResult>::failure(
            core::StatusCode::InvalidInput,
            "Thermal conductivity must be positive and finite (provided: " + std::to_string(params.thermal_conductivity) + ")."
        );
    }

    if (!std::isfinite(params.heat_generation)) {
        return core::Result<Fem1DResult>::failure(
            core::StatusCode::InvalidInput,
            "Heat generation must be finite."
        );
    }

    if (!std::isfinite(params.left_temperature) || !std::isfinite(params.right_temperature)) {
        return core::Result<Fem1DResult>::failure(
            core::StatusCode::InvalidInput,
            "Boundary temperatures must be finite."
        );
    }

    if (params.element_count == 0) {
        return core::Result<Fem1DResult>::failure(
            core::StatusCode::InvalidInput,
            "Element count must be at least 1 (provided: 0)."
        );
    }

    const size_t N = params.element_count;
    const size_t num_nodes = N + 1;
    const double h = params.length / static_cast<double>(N);

    Fem1DResult result;
    result.parameters = params;
    result.element_length = h;

    std::vector<double> nodal_temperatures(num_nodes, 0.0);
    nodal_temperatures[0] = params.left_temperature;
    nodal_temperatures[N] = params.right_temperature;

    // Element conductance scalar: Ke = (k * A / h)
    const double K_elem = (params.thermal_conductivity * params.area) / h;
    // Nodal heat load contribution: Fe_node = (q * A * h) / 2
    // Interior nodes receive contributions from two adjacent elements: F_node = q * A * h
    const double F_interior = params.heat_generation * params.area * h;

    // 2. Interior System Assembly & Solution (N > 1)
    if (N > 1) {
        const size_t num_unknowns = N - 1; // Nodes 1 to N-1

        // Tridiagonal matrix diagonals for interior unknowns:
        // sub-diagonal a (size num_unknowns - 1)
        // main-diagonal b (size num_unknowns)
        // super-diagonal c (size num_unknowns - 1)
        // right-hand side d (size num_unknowns)
        std::vector<double> a(num_unknowns - 1, -K_elem);
        std::vector<double> b(num_unknowns, 2.0 * K_elem);
        std::vector<double> c(num_unknowns - 1, -K_elem);
        std::vector<double> d(num_unknowns, F_interior);

        // Apply Dirichlet boundary conditions onto the RHS:
        // For node 1: -K_elem * T_0 + 2*K_elem * T_1 - K_elem * T_2 = F_interior
        // => 2*K_elem * T_1 - K_elem * T_2 = F_interior + K_elem * T_0
        d[0] += K_elem * params.left_temperature;

        // For node N-1: -K_elem * T_{N-2} + 2*K_elem * T_{N-1} - K_elem * T_N = F_interior
        // => -K_elem * T_{N-2} + 2*K_elem * T_{N-1} = F_interior + K_elem * T_N
        d[num_unknowns - 1] += K_elem * params.right_temperature;

        // 3. Thomas Algorithm (Tridiagonal Matrix Algorithm - TDMA)
        // Forward elimination
        for (size_t i = 1; i < num_unknowns; ++i) {
            if (std::abs(b[i - 1]) < 1.0e-15) {
                return core::Result<Fem1DResult>::failure(
                    core::StatusCode::ComputationError,
                    "Thomas algorithm failed: zero or near-zero pivot encountered at row " + std::to_string(i - 1) + "."
                );
            }
            const double factor = a[i - 1] / b[i - 1];
            b[i] -= factor * c[i - 1];
            d[i] -= factor * d[i - 1];
        }

        if (std::abs(b[num_unknowns - 1]) < 1.0e-15) {
            return core::Result<Fem1DResult>::failure(
                core::StatusCode::ComputationError,
                "Thomas algorithm failed: zero pivot at final row."
            );
        }

        // Back substitution
        nodal_temperatures[N - 1] = d[num_unknowns - 1] / b[num_unknowns - 1];
        for (size_t i = num_unknowns - 1; i > 0; --i) {
            const size_t idx = i - 1;
            nodal_temperatures[idx + 1] = (d[idx] - c[idx] * nodal_temperatures[idx + 2]) / b[idx];
        }
    }

    // 4. Verify Finiteness of Solution
    for (size_t i = 0; i < num_nodes; ++i) {
        if (!std::isfinite(nodal_temperatures[i])) {
            return core::Result<Fem1DResult>::failure(
                core::StatusCode::ComputationError,
                "Non-finite temperature solution produced at node " + std::to_string(i) + "."
            );
        }
    }

    // 5. Build Nodal Results
    result.nodes.resize(num_nodes);
    for (size_t i = 0; i < num_nodes; ++i) {
        FemNodeResult node;
        node.node_index = i;
        node.x_m = static_cast<double>(i) * h;
        node.temperature_FE_C = nodal_temperatures[i];
        node.temperature_exact_C = compute_exact_temperature(node.x_m, params);
        node.absolute_error_C = std::abs(node.temperature_FE_C - node.temperature_exact_C);
        result.nodes[i] = node;
    }

    // 6. Build Element Midpoint Results & Convergence Errors
    result.elements.resize(N);
    double max_err = 0.0;

    for (size_t e = 0; e < N; ++e) {
        FemElementResult elem;
        elem.element_index = e;
        elem.node_left = e;
        elem.node_right = e + 1;
        elem.x_mid_m = (static_cast<double>(e) + 0.5) * h;

        // Linear shape function interpolation at element midpoint: T_mid = (T_left + T_right) / 2
        elem.temperature_FE_mid_C = 0.5 * (nodal_temperatures[e] + nodal_temperatures[e + 1]);
        elem.temperature_exact_mid_C = compute_exact_temperature(elem.x_mid_m, params);
        elem.absolute_error_C = std::abs(elem.temperature_exact_mid_C - elem.temperature_FE_mid_C);

        max_err = std::max(max_err, elem.absolute_error_C);
        result.elements[e] = elem;
    }

    result.max_absolute_midpoint_error_C = max_err;
    // Theoretical error: (q / (8 * k)) * h^2
    result.expected_max_midpoint_error_C = (params.heat_generation / (8.0 * params.thermal_conductivity)) * h * h;

    return core::Result<Fem1DResult>::success(std::move(result));
}

core::Status export_fem_results_to_csv(const std::string& filepath, const Fem1DResult& result) {
    if (filepath.empty()) {
        return core::Status::failure(
            core::StatusCode::InvalidInput,
            "Cannot export FEM results: output filepath is empty."
        );
    }

    std::ofstream out(filepath);
    if (!out.is_open()) {
        return core::Status::failure(
            core::StatusCode::FileNotFound,
            "Failed to open or create CSV output file: " + filepath
        );
    }

    // Header with required units
    out << "x_m,temperature_FE_C,temperature_exact_C,absolute_error_C\n";

    // Struct for sorting and writing combined points (nodes and midpoints)
    struct OutputPoint {
        double x_m;
        double temperature_FE_C;
        double temperature_exact_C;
        double absolute_error_C;
    };

    std::vector<OutputPoint> points;
    points.reserve(result.nodes.size() + result.elements.size());

    // Add nodes
    for (const auto& node : result.nodes) {
        points.push_back({node.x_m, node.temperature_FE_C, node.temperature_exact_C, node.absolute_error_C});
    }

    // Add element midpoints
    for (const auto& elem : result.elements) {
        points.push_back({elem.x_mid_m, elem.temperature_FE_mid_C, elem.temperature_exact_mid_C, elem.absolute_error_C});
    }

    // Sort ascending by x_m
    std::sort(points.begin(), points.end(), [](const OutputPoint& a, const OutputPoint& b) {
        return a.x_m < b.x_m;
    });

    out << std::fixed << std::setprecision(8);
    for (const auto& pt : points) {
        out << pt.x_m << ","
            << pt.temperature_FE_C << ","
            << pt.temperature_exact_C << ","
            << pt.absolute_error_C << "\n";
    }

    if (out.fail()) {
        return core::Status::failure(
            core::StatusCode::ComputationError,
            "Stream write failure occurred while writing to CSV: " + filepath
        );
    }

    return core::Status::ok();
}

} // namespace workbench::thermal
