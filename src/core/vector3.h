#pragma once

#include <cmath>

namespace workbench::core {

/**
 * @brief Represents a 3D vector or spatial coordinate (x, y, z).
 * 
 * Used across geometry parsing, nodal coordinates, mesh diagnostics,
 * and vector mathematics.
 */
struct Vector3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vector3D() = default;
    constexpr Vector3D(double in_x, double in_y, double in_z)
        : x(in_x), y(in_y), z(in_z) {}

    // Vector addition: a + b
    Vector3D operator+(const Vector3D& other) const noexcept {
        return Vector3D{x + other.x, y + other.y, z + other.z};
    }

    // Vector subtraction: a - b
    Vector3D operator-(const Vector3D& other) const noexcept {
        return Vector3D{x - other.x, y - other.y, z - other.z};
    }

    // Scalar multiplication: v * s
    Vector3D operator*(double scalar) const noexcept {
        return Vector3D{x * scalar, y * scalar, z * scalar};
    }

    // Scalar division: v / s
    Vector3D operator/(double scalar) const {
        return Vector3D{x / scalar, y / scalar, z / scalar};
    }

    // Dot product: a · b
    double dot(const Vector3D& other) const noexcept {
        return x * other.x + y * other.y + z * other.z;
    }

    // Cross product: a × b
    Vector3D cross(const Vector3D& other) const noexcept {
        return Vector3D{
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        };
    }

    // Euclidean length: ||v|| = sqrt(x^2 + y^2 + z^2)
    double magnitude() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }

    // Squared magnitude (useful for avoiding square roots during distance comparisons)
    double magnitude_squared() const noexcept {
        return x * x + y * y + z * z;
    }

    // Returns a unit-length vector with magnitude 1.0
    Vector3D normalized() const {
        const double mag = magnitude();
        if (mag == 0.0) {
            return Vector3D{0.0, 0.0, 0.0};
        }
        return *this / mag;
    }

    // Euclidean distance between this point and another point
    double distance_to(const Vector3D& other) const noexcept {
        return (*this - other).magnitude();
    }

    // Returns true if all three components are finite (neither NaN nor Inf)
    bool is_finite() const noexcept {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }
};

// Enables scalar multiplication from the left: s * v
inline Vector3D operator*(double scalar, const Vector3D& v) noexcept {
    return v * scalar;
}

} // namespace workbench::core
