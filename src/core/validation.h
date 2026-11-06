#pragma once

#include "core/vector3.h"
#include <cmath>

namespace workbench::core {

/**
 * @brief Engineering validation and unit conversion utilities.
 */
namespace validation {

/**
 * @brief Checks if two floating-point numbers are approximately equal within an absolute epsilon.
 */
inline bool approx_equal(double a, double b, double epsilon = 1e-6) noexcept {
    return std::abs(a - b) <= epsilon;
}

/**
 * @brief Minimum physically possible temperature in degrees Celsius (Absolute Zero).
 */
constexpr double ABSOLUTE_ZERO_CELSIUS = -273.15;

/**
 * @brief Validates if a temperature value is physically plausible (above absolute zero and finite).
 */
bool is_physically_valid_temperature_celsius(double temp_c) noexcept;

/**
 * @brief Converts temperature from Celsius to Kelvin.
 */
double celsius_to_kelvin(double temp_c) noexcept;

/**
 * @brief Converts temperature from Kelvin to Celsius.
 */
double kelvin_to_celsius(double temp_k) noexcept;

/**
 * @brief Computes a scale-relative geometric tolerance based on bounding box diagonal.
 * 
 * @param bounding_box_diagonal The spatial diagonal length of the geometry bounding box.
 * @param factor Scale factor (default: 1.0e-7).
 * @return double Relative geometric tolerance value.
 */
double compute_scale_relative_tolerance(double bounding_box_diagonal, double factor = 1e-7) noexcept;

/**
 * @brief Checks if a triangle facet is degenerate (zero or near-zero area) relative to mesh scale.
 * 
 * @param v1 First vertex coordinate.
 * @param v2 Second vertex coordinate.
 * @param v3 Third vertex coordinate.
 * @param bounding_box_diagonal The spatial diagonal length of the entire mesh bounding box.
 * @return true if the facet is degenerate, false if it is a valid non-zero surface element.
 */
bool is_triangle_degenerate(const Vector3D& v1,
                            const Vector3D& v2,
                            const Vector3D& v3,
                            double bounding_box_diagonal) noexcept;

} // namespace validation
} // namespace workbench::core
