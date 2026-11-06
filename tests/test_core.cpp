#include <iostream>
#include <cassert>
#include <cmath>
#include <limits>
#include <string>

#include "core/workbench_core.h"
#include "core/vector3.h"
#include "core/bounding_box.h"
#include "core/result.h"
#include "core/validation.h"
#include "core/cancellation_token.h"

using namespace workbench::core;

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(expr, msg) \
    do { \
        if (expr) { \
            g_tests_passed++; \
        } else { \
            g_tests_failed++; \
            std::cerr << "  [FAIL] " << msg << " (" << #expr << ") at line " << __LINE__ << "\n"; \
        } \
    } while (0)

void test_workbench_core_info() {
    std::cout << "[TEST GROUP] Workbench Core Foundation Info...\n";
    TEST_ASSERT(get_application_name() == "UNIFIED ENGINEERING WORKBENCH", "Application name matches expected");
    TEST_ASSERT(get_version_string() == "1.0.0", "Version string is 1.0.0");
    TEST_ASSERT(initialize_core() == true, "Core initialization succeeds");
}

void test_vector3d_operations() {
    std::cout << "[TEST GROUP] Vector3D Operations...\n";

    const Vector3D a{1.0, 2.0, 3.0};
    const Vector3D b{4.0, -5.0, 6.0};

    // Addition & Subtraction
    const Vector3D sum = a + b;
    TEST_ASSERT(sum.x == 5.0 && sum.y == -3.0 && sum.z == 9.0, "Vector addition correct");

    const Vector3D diff = a - b;
    TEST_ASSERT(diff.x == -3.0 && diff.y == 7.0 && diff.z == -3.0, "Vector subtraction correct");

    // Scalar operations
    const Vector3D scaled = a * 2.5;
    TEST_ASSERT(scaled.x == 2.5 && scaled.y == 5.0 && scaled.z == 7.5, "Scalar multiplication correct");

    const Vector3D left_scaled = 2.0 * a;
    TEST_ASSERT(left_scaled.x == 2.0 && left_scaled.y == 4.0 && left_scaled.z == 6.0, "Left scalar multiplication correct");

    const Vector3D divided = a / 2.0;
    TEST_ASSERT(divided.x == 0.5 && divided.y == 1.0 && divided.z == 1.5, "Scalar division correct");

    // Dot product: 1*4 + 2*(-5) + 3*6 = 4 - 10 + 18 = 12
    const double dot_prod = a.dot(b);
    TEST_ASSERT(validation::approx_equal(dot_prod, 12.0), "Dot product correct");

    // Cross product: i x j = k
    const Vector3D unit_x{1.0, 0.0, 0.0};
    const Vector3D unit_y{0.0, 1.0, 0.0};
    const Vector3D cross_xy = unit_x.cross(unit_y);
    TEST_ASSERT(validation::approx_equal(cross_xy.x, 0.0) &&
                validation::approx_equal(cross_xy.y, 0.0) &&
                validation::approx_equal(cross_xy.z, 1.0), "Cross product i x j = k correct");

    // Magnitude: sqrt(3^2 + 4^2 + 0^2) = 5
    const Vector3D pythagorean{3.0, 4.0, 0.0};
    TEST_ASSERT(validation::approx_equal(pythagorean.magnitude(), 5.0), "Magnitude calculation correct");
    TEST_ASSERT(validation::approx_equal(pythagorean.magnitude_squared(), 25.0), "Squared magnitude correct");

    // Normalization
    const Vector3D normalized = pythagorean.normalized();
    TEST_ASSERT(validation::approx_equal(normalized.magnitude(), 1.0), "Normalized vector length is 1.0");
    TEST_ASSERT(validation::approx_equal(normalized.x, 0.6) &&
                validation::approx_equal(normalized.y, 0.8), "Normalized components correct");

    // Distance
    const Vector3D p1{0.0, 0.0, 0.0};
    const Vector3D p2{0.0, 0.0, 10.0};
    TEST_ASSERT(validation::approx_equal(p1.distance_to(p2), 10.0), "Distance calculation correct");

    // Finite check
    TEST_ASSERT(a.is_finite() == true, "Normal vector is finite");
    const Vector3D nan_vec{std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0};
    TEST_ASSERT(nan_vec.is_finite() == false, "NaN vector is not finite");
}

void test_bounding_box() {
    std::cout << "[TEST GROUP] BoundingBox3D Operations...\n";

    BoundingBox3D bbox;
    TEST_ASSERT(bbox.is_empty() == true, "Initial bounding box is empty");
    TEST_ASSERT(bbox.is_zero_diagonal() == true, "Empty bounding box has zero diagonal");

    // Include single point
    bbox.include_point(Vector3D{5.0, 5.0, 5.0});
    TEST_ASSERT(bbox.is_empty() == false, "Bounding box is not empty after adding point");
    TEST_ASSERT(bbox.is_zero_diagonal() == true, "Single-point bounding box has exactly zero diagonal");
    TEST_ASSERT(validation::approx_equal(bbox.diagonal_length(), 0.0), "Single-point diagonal length is 0.0");

    // Include second point to create a 3x4x0 box
    bbox.include_point(Vector3D{8.0, 9.0, 5.0});
    TEST_ASSERT(bbox.is_zero_diagonal() == false, "Multi-point bounding box has non-zero diagonal");
    const Vector3D dims = bbox.dimensions();
    TEST_ASSERT(validation::approx_equal(dims.x, 3.0), "Dimension dx is 3.0");
    TEST_ASSERT(validation::approx_equal(dims.y, 4.0), "Dimension dy is 4.0");
    TEST_ASSERT(validation::approx_equal(dims.z, 0.0), "Dimension dz is 0.0");
    TEST_ASSERT(validation::approx_equal(bbox.diagonal_length(), 5.0), "Diagonal length is 5.0 (Pythagorean 3-4-5)");

    const Vector3D center = bbox.center();
    TEST_ASSERT(validation::approx_equal(center.x, 6.5) &&
                validation::approx_equal(center.y, 7.0) &&
                validation::approx_equal(center.z, 5.0), "Center coordinates correct");
}

void test_result_and_status() {
    std::cout << "[TEST GROUP] Result<T> and Status Types...\n";

    // Result Success
    const auto res_ok = Result<double>::success(42.5);
    TEST_ASSERT(res_ok.is_ok() == true, "Result is_ok() true for success");
    TEST_ASSERT(res_ok.is_error() == false, "Result is_error() false for success");
    TEST_ASSERT(res_ok.value() == 42.5, "Result value matches stored value");

    // Result Failure
    const auto res_err = Result<double>::failure(StatusCode::InvalidInput, "Sample error");
    TEST_ASSERT(res_err.is_ok() == false, "Result is_ok() false for failure");
    TEST_ASSERT(res_err.is_error() == true, "Result is_error() true for failure");
    TEST_ASSERT(res_err.error().code == StatusCode::InvalidInput, "Error code matches");
    TEST_ASSERT(res_err.error().message == "Sample error", "Error message matches");

    // Status
    const auto status_ok = Status::ok();
    TEST_ASSERT(status_ok.is_ok() == true, "Status::ok() is_ok() true");

    const auto status_err = Status::failure(StatusCode::ComputationError, "Convergence failed");
    TEST_ASSERT(status_err.is_error() == true, "Status::failure() is_error() true");
    TEST_ASSERT(status_err.error().code == StatusCode::ComputationError, "Status error code matches");
}

void test_validation_utilities() {
    std::cout << "[TEST GROUP] Validation Utilities & Temperature Conversion...\n";

    // Approx equal
    TEST_ASSERT(validation::approx_equal(1.0000001, 1.0000002, 1e-5) == true, "approx_equal detects close values");
    TEST_ASSERT(validation::approx_equal(1.0, 1.1, 1e-3) == false, "approx_equal rejects differing values");

    // Temperature conversions
    TEST_ASSERT(validation::approx_equal(validation::celsius_to_kelvin(0.0), 273.15), "0 C is 273.15 K");
    TEST_ASSERT(validation::approx_equal(validation::celsius_to_kelvin(100.0), 373.15), "100 C is 373.15 K");
    TEST_ASSERT(validation::approx_equal(validation::kelvin_to_celsius(373.15), 100.0), "373.15 K is 100 C");

    // Temperature physical bounds
    TEST_ASSERT(validation::is_physically_valid_temperature_celsius(25.0) == true, "Room temperature is valid");
    TEST_ASSERT(validation::is_physically_valid_temperature_celsius(-273.15) == true, "Absolute zero is valid boundary");
    TEST_ASSERT(validation::is_physically_valid_temperature_celsius(-300.0) == false, "Below absolute zero is invalid");
    TEST_ASSERT(validation::is_physically_valid_temperature_celsius(std::numeric_limits<double>::quiet_NaN()) == false, "NaN temperature is invalid");

    // Scale-relative tolerance
    const double diag = 100.0;
    const double rel_tol = validation::compute_scale_relative_tolerance(diag, 1e-7);
    TEST_ASSERT(validation::approx_equal(rel_tol, 1e-5), "Scale relative tolerance calculation correct");

    // Scale-relative triangle degeneracy
    // Non-degenerate triangle in a 10m x 10m mesh
    const Vector3D v1{0.0, 0.0, 0.0};
    const Vector3D v2{1.0, 0.0, 0.0};
    const Vector3D v3{0.0, 1.0, 0.0};
    TEST_ASSERT(validation::is_triangle_degenerate(v1, v2, v3, 10.0) == false, "Right triangle is not degenerate");

    // Collinear degenerate triangle (all on X axis)
    const Vector3D c1{0.0, 0.0, 0.0};
    const Vector3D c2{1.0, 0.0, 0.0};
    const Vector3D c3{2.0, 0.0, 0.0};
    TEST_ASSERT(validation::is_triangle_degenerate(c1, c2, c3, 10.0) == true, "Collinear triangle is degenerate");

    // Coincident vertices
    TEST_ASSERT(validation::is_triangle_degenerate(v1, v1, v1, 10.0) == true, "Coincident vertices triangle is degenerate");

    // Zero-diagonal bounding box (entire mesh degenerate)
    TEST_ASSERT(validation::is_triangle_degenerate(v1, v2, v3, 0.0) == true, "Zero diagonal bounding box flags degeneracy");
}

void test_cancellation_token() {
    std::cout << "[TEST GROUP] CancellationToken Thread-Safety...\n";

    CancellationToken token;
    TEST_ASSERT(token.is_cancelled() == false, "Token initially not cancelled");

    token.request_cancellation();
    TEST_ASSERT(token.is_cancelled() == true, "Token reports cancelled after request");

    token.reset();
    TEST_ASSERT(token.is_cancelled() == false, "Token reports not cancelled after reset");
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  UNIFIED ENGINEERING WORKBENCH TESTS  \n";
    std::cout << "  C++17 Engineering Core Module Unit Tests\n";
    std::cout << "========================================\n\n";

    test_workbench_core_info();
    test_vector3d_operations();
    test_bounding_box();
    test_result_and_status();
    test_validation_utilities();
    test_cancellation_token();

    std::cout << "\n========================================\n";
    std::cout << "  TEST SUMMARY: " << g_tests_passed << " PASSED, " << g_tests_failed << " FAILED\n";
    std::cout << "========================================\n";

    return (g_tests_failed == 0) ? 0 : 1;
}
