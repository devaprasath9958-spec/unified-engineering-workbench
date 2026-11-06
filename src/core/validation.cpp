#include "core/validation.h"
#include <cmath>

namespace workbench::core::validation {

bool is_physically_valid_temperature_celsius(double temp_c) noexcept {
    return std::isfinite(temp_c) && (temp_c >= ABSOLUTE_ZERO_CELSIUS);
}

double celsius_to_kelvin(double temp_c) noexcept {
    return temp_c + 273.15;
}

double kelvin_to_celsius(double temp_k) noexcept {
    return temp_k - 273.15;
}

double compute_scale_relative_tolerance(double bounding_box_diagonal, double factor) noexcept {
    if (bounding_box_diagonal <= 0.0 || !std::isfinite(bounding_box_diagonal)) {
        return 0.0;
    }
    return factor * bounding_box_diagonal;
}

bool is_triangle_degenerate(const Vector3D& v1,
                            const Vector3D& v2,
                            const Vector3D& v3,
                            double bounding_box_diagonal) noexcept {
    // If bounding box has exactly zero diagonal, the entire geometry is degenerate
    if (bounding_box_diagonal <= 0.0 || !std::isfinite(bounding_box_diagonal)) {
        return true;
    }

    // Verify all vertex coordinates are finite
    if (!v1.is_finite() || !v2.is_finite() || !v3.is_finite()) {
        return true;
    }

    // Edge vectors of the triangle facet
    const Vector3D e1 = v2 - v1;
    const Vector3D e2 = v3 - v1;

    // The cross product magnitude is twice the triangle area: ||e1 x e2|| = 2 * Area
    const Vector3D normal_cross = e1.cross(e2);
    const double cross_mag = normal_cross.magnitude();

    // Scale-relative threshold: eps_geom = 1e-7 * D_bbox
    const double eps_geom = compute_scale_relative_tolerance(bounding_box_diagonal, 1.0e-7);
    const double threshold = eps_geom * eps_geom;

    return cross_mag < threshold;
}

} // namespace workbench::core::validation
