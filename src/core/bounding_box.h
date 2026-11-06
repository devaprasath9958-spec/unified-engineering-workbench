#pragma once

#include "core/vector3.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace workbench::core {

/**
 * @brief Represents an Axis-Aligned Bounding Box (AABB) in 3D space.
 * 
 * Tracks spatial bounds of geometries and point clouds, calculates bounding
 * diagonals, and determines whether an extent is zero.
 */
class BoundingBox3D {
public:
    BoundingBox3D()
        : min_point(std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity()),
          max_point(-std::numeric_limits<double>::infinity(),
                    -std::numeric_limits<double>::infinity(),
                    -std::numeric_limits<double>::infinity()),
          m_is_empty(true) {}

    BoundingBox3D(const Vector3D& min_pt, const Vector3D& max_pt)
        : min_point(min_pt), max_point(max_pt), m_is_empty(false) {}

    // Expands the bounding box to enclose the specified 3D point
    void include_point(const Vector3D& pt) noexcept {
        min_point.x = std::min(min_point.x, pt.x);
        min_point.y = std::min(min_point.y, pt.y);
        min_point.z = std::min(min_point.z, pt.z);

        max_point.x = std::max(max_point.x, pt.x);
        max_point.y = std::max(max_point.y, pt.y);
        max_point.z = std::max(max_point.z, pt.z);

        m_is_empty = false;
    }

    // Returns true if no points have been added to this bounding box
    bool is_empty() const noexcept {
        return m_is_empty;
    }

    // Calculates box dimensions (dx, dy, dz)
    Vector3D dimensions() const noexcept {
        if (m_is_empty) {
            return Vector3D{0.0, 0.0, 0.0};
        }
        return Vector3D{
            max_point.x - min_point.x,
            max_point.y - min_point.y,
            max_point.z - min_point.z
        };
    }

    // Calculates the Euclidean length of the spatial diagonal
    double diagonal_length() const noexcept {
        if (m_is_empty) {
            return 0.0;
        }
        return dimensions().magnitude();
    }

    // Calculates the geometric center of the bounding box
    Vector3D center() const noexcept {
        if (m_is_empty) {
            return Vector3D{0.0, 0.0, 0.0};
        }
        return (min_point + max_point) * 0.5;
    }

    // Returns true if all points are coincident or bounding box has exactly zero diagonal
    bool is_zero_diagonal() const noexcept {
        if (m_is_empty) {
            return true;
        }
        return diagonal_length() == 0.0;
    }

    Vector3D min_point;
    Vector3D max_point;

private:
    bool m_is_empty = true;
};

} // namespace workbench::core
