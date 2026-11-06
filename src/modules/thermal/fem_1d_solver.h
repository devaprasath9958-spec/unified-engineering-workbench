#pragma once

#include "core/result.h"
#include <vector>
#include <string>
#include <cstddef>

namespace workbench::thermal {

/**
 * @brief Physical and numerical configuration for 1D steady-state thermal FEM.
 */
struct Thermal1DParameters {
    double length = 1.0;                  // Rod length (L) [m]
    double area = 1.0e-4;                 // Cross-sectional area (A) [m^2]
    double thermal_conductivity = 10.0;   // Thermal conductivity (k) [W/(m·K)]
    double heat_generation = 10000.0;     // Uniform volumetric heat generation (q) [W/m^3]
    double left_temperature = 100.0;      // Boundary temperature at x = 0 (T_0) [°C]
    double right_temperature = 20.0;      // Boundary temperature at x = L (T_L) [°C]
    size_t element_count = 10;            // Number of two-node linear elements (N)
};

/**
 * @brief Temperature solution evaluated at an element node.
 */
struct FemNodeResult {
    size_t node_index = 0;
    double x_m = 0.0;
    double temperature_FE_C = 0.0;
    double temperature_exact_C = 0.0;
    double absolute_error_C = 0.0;
};

/**
 * @brief Temperature evaluation at an element midpoint via linear interpolation.
 */
struct FemElementResult {
    size_t element_index = 0;
    size_t node_left = 0;
    size_t node_right = 0;
    double x_mid_m = 0.0;
    double temperature_FE_mid_C = 0.0;
    double temperature_exact_mid_C = 0.0;
    double absolute_error_C = 0.0;
};

/**
 * @brief Comprehensive simulation result of 1D steady-state thermal FEM analysis.
 */
struct Fem1DResult {
    Thermal1DParameters parameters;
    double element_length = 0.0;                 // h = L / N [m]
    std::vector<FemNodeResult> nodes;            // Nodal temperature results (N+1 nodes)
    std::vector<FemElementResult> elements;      // Midpoint evaluations (N elements)
    double max_absolute_midpoint_error_C = 0.0; // Maximum midpoint error across elements [°C]
    double expected_max_midpoint_error_C = 0.0; // Theoretical midpoint error: (q/(8*k))*h^2 [°C]
};

/**
 * @brief Computes the analytical exact temperature at spatial position x.
 * 
 * T_exact(x) = T(0) + (T(L) - T(0))*x/L + (q / (2*k)) * x * (L - x)
 * For L=1, k=10, q=10000, T(0)=100, T(L)=20: T_exact(x) = 100 + 420*x - 500*x^2
 * 
 * @param x Spatial position along the rod [m].
 * @param params Rod physical and thermal parameters.
 * @return double Exact analytical temperature [°C].
 */
double compute_exact_temperature(double x, const Thermal1DParameters& params) noexcept;

/**
 * @brief Solves steady-state 1D heat conduction using two-node linear finite elements.
 * 
 * Sets up uniform 1D mesh, assembles global conductance matrix and load vector,
 * enforces Dirichlet boundary conditions at both ends, solves the tridiagonal system
 * using the Thomas algorithm, and computes midpoint errors and convergence metrics.
 * 
 * @param params The physical parameters and element count.
 * @return core::Result<Fem1DResult> Solved FEM results or an Error description.
 */
core::Result<Fem1DResult> solve_steady_state_1d(const Thermal1DParameters& params);

/**
 * @brief Exports FEM results to a CSV file.
 * 
 * Generates rows for nodes and element midpoints with exact required columns:
 * x_m,temperature_FE_C,temperature_exact_C,absolute_error_C
 * 
 * @param filepath Destination file path on disk.
 * @param result The solved FEM results to export.
 * @return core::Status Ok on success, or Failure with useful error message on I/O failure.
 */
core::Status export_fem_results_to_csv(const std::string& filepath, const Fem1DResult& result);

} // namespace workbench::thermal
