#include <iostream>
#include <cassert>
#include <cmath>
#include <string>
#include <filesystem>

#include "modules/geometry/stl_parser.h"
#include "modules/geometry/csv_parser.h"
#include "core/validation.h"

using namespace workbench::geometry;
using namespace workbench::core;

static int g_geo_passed = 0;
static int g_geo_failed = 0;

#define GEO_ASSERT(expr, msg) \
    do { \
        if (expr) { \
            g_geo_passed++; \
        } else { \
            g_geo_failed++; \
            std::cerr << "  [FAIL] " << msg << " (" << #expr << ") at line " << __LINE__ << "\n"; \
        } \
    } while (0)

// Helper to resolve sample files whether executed from project root, build/, or build/Release/
static std::string resolve_sample_path(const std::string& rel_path) {
    if (std::filesystem::exists(rel_path)) {
        return rel_path;
    }
    if (std::filesystem::exists("../" + rel_path)) {
        return "../" + rel_path;
    }
    if (std::filesystem::exists("../../" + rel_path)) {
        return "../../" + rel_path;
    }
    return rel_path;
}

void test_ascii_stl_valid_triangle() {
    std::cout << "[TEST GROUP] ASCII STL Valid Single Triangle...\n";

    const auto res = parse_ascii_stl_file(resolve_sample_path("data/samples/valid_triangle.stl"));
    GEO_ASSERT(res.is_ok(), "Parsing valid_triangle.stl succeeds");

    if (res.is_ok()) {
        const auto& mesh = res.value();
        GEO_ASSERT(mesh.solid_name == "sample_single_triangle", "Solid name parsed correctly");
        GEO_ASSERT(mesh.triangle_count() == 1, "Exactly 1 triangle parsed");
        GEO_ASSERT(mesh.degenerate_count == 0, "Zero degenerate triangles detected");

        const auto& tri = mesh.triangles[0];
        GEO_ASSERT(validation::approx_equal(tri.normal.x, 0.0) &&
                   validation::approx_equal(tri.normal.y, 0.0) &&
                   validation::approx_equal(tri.normal.z, 1.0), "Facet normal is (0,0,1)");

        GEO_ASSERT(validation::approx_equal(tri.area(), 0.5), "Right-triangle facet area is 0.5");
        GEO_ASSERT(validation::approx_equal(mesh.total_surface_area, 0.5), "Total surface area is 0.5");

        // Bounding box checks
        const auto& bbox = mesh.bounding_box;
        GEO_ASSERT(validation::approx_equal(bbox.min_point.x, 0.0) &&
                   validation::approx_equal(bbox.min_point.y, 0.0) &&
                   validation::approx_equal(bbox.min_point.z, 0.0), "Bounding box min is (0,0,0)");
        GEO_ASSERT(validation::approx_equal(bbox.max_point.x, 1.0) &&
                   validation::approx_equal(bbox.max_point.y, 1.0) &&
                   validation::approx_equal(bbox.max_point.z, 0.0), "Bounding box max is (1,1,0)");
        GEO_ASSERT(validation::approx_equal(bbox.diagonal_length(), std::sqrt(2.0)), "Bounding box diagonal is sqrt(2)");
        GEO_ASSERT(bbox.is_zero_diagonal() == false, "Bounding box has non-zero diagonal");
    }
}

void test_ascii_stl_bracket_mesh() {
    std::cout << "[TEST GROUP] ASCII STL Multi-Facet Bracket Mesh...\n";

    const auto res = parse_ascii_stl_file(resolve_sample_path("data/samples/valid_bracket.stl"));
    GEO_ASSERT(res.is_ok(), "Parsing valid_bracket.stl succeeds");

    if (res.is_ok()) {
        const auto& mesh = res.value();
        GEO_ASSERT(mesh.solid_name == "l_bracket_segment", "Solid name parsed correctly");
        GEO_ASSERT(mesh.triangle_count() == 4, "Mesh contains 4 triangular facets");
        GEO_ASSERT(mesh.degenerate_count == 0, "No degenerate facets in valid bracket");

        const auto& bbox = mesh.bounding_box;
        GEO_ASSERT(validation::approx_equal(bbox.min_point.x, 0.0) &&
                   validation::approx_equal(bbox.min_point.y, 0.0) &&
                   validation::approx_equal(bbox.min_point.z, 0.0), "Bounding box min is (0,0,0)");
        GEO_ASSERT(validation::approx_equal(bbox.max_point.x, 10.0) &&
                   validation::approx_equal(bbox.max_point.y, 10.0) &&
                   validation::approx_equal(bbox.max_point.z, 5.0), "Bounding box max is (10,10,5)");

        // Diagonal = sqrt(10^2 + 10^2 + 5^2) = sqrt(225) = 15.0
        GEO_ASSERT(validation::approx_equal(bbox.diagonal_length(), 15.0), "Bounding box diagonal is exactly 15.0");

        const auto center = bbox.center();
        GEO_ASSERT(validation::approx_equal(center.x, 5.0) &&
                   validation::approx_equal(center.y, 5.0) &&
                   validation::approx_equal(center.z, 2.5), "Bounding box center is (5, 5, 2.5)");

        GEO_ASSERT(mesh.total_surface_area > 100.0, "Total surface area is positive and physically non-zero");
    }
}

void test_ascii_stl_degenerate_detection() {
    std::cout << "[TEST GROUP] ASCII STL Degenerate Triangle Detection...\n";

    const auto res = parse_ascii_stl_file(resolve_sample_path("data/samples/degenerate_facet.stl"));
    GEO_ASSERT(res.is_ok(), "Parsing degenerate_facet.stl succeeds");

    if (res.is_ok()) {
        const auto& mesh = res.value();
        GEO_ASSERT(mesh.triangle_count() == 2, "Mesh contains 2 triangles");
        GEO_ASSERT(mesh.degenerate_count == 1, "Exactly 1 degenerate triangle detected");
        GEO_ASSERT(mesh.triangles[0].is_degenerate == true, "First facet (collinear) flagged as degenerate");
        GEO_ASSERT(mesh.triangles[1].is_degenerate == false, "Second facet (valid) not flagged as degenerate");
    }
}

void test_ascii_stl_error_handling() {
    std::cout << "[TEST GROUP] ASCII STL Error Handling & Rejection...\n";

    // 1. Non-existent file
    const auto res_missing = parse_ascii_stl_file("data/samples/non_existent_file.stl");
    GEO_ASSERT(res_missing.is_error(), "Missing file returns error");
    GEO_ASSERT(res_missing.error().code == StatusCode::FileNotFound, "Missing file code is FileNotFound");

    // 2. Empty file (0 bytes)
    const auto res_empty = parse_ascii_stl_file(resolve_sample_path("data/samples/empty.stl"));
    GEO_ASSERT(res_empty.is_error(), "Empty STL returns error");
    GEO_ASSERT(res_empty.error().code == StatusCode::InvalidInput, "Empty STL code is InvalidInput");

    // 3. Missing solid header
    const auto res_no_solid = parse_ascii_stl_file(resolve_sample_path("data/samples/malformed_no_solid.stl"));
    GEO_ASSERT(res_no_solid.is_error(), "STL missing solid header returns error");
    GEO_ASSERT(res_no_solid.error().code == StatusCode::InvalidInput, "Missing solid code is InvalidInput");

    // 4. Incomplete facet loop (missing 3rd vertex)
    const auto res_missing_v = parse_ascii_stl_file(resolve_sample_path("data/samples/malformed_missing_vertex.stl"));
    GEO_ASSERT(res_missing_v.is_error(), "STL with incomplete loop returns error");
    GEO_ASSERT(res_missing_v.error().code == StatusCode::InvalidInput, "Incomplete loop code is InvalidInput");

    // 5. Non-finite coordinate (NaN)
    const auto res_nan = parse_ascii_stl_file(resolve_sample_path("data/samples/malformed_nan.stl"));
    GEO_ASSERT(res_nan.is_error(), "STL with NaN coordinate returns error");
    GEO_ASSERT(res_nan.error().code == StatusCode::InvalidInput, "NaN coordinate code is InvalidInput");

    // 6. Zero triangles (empty solid)
    const std::string empty_solid = "solid empty_test\nendsolid empty_test\n";
    const auto res_zero_tri = parse_ascii_stl_string(empty_solid);
    GEO_ASSERT(res_zero_tri.is_error(), "Empty solid with zero triangles returns error");
    GEO_ASSERT(res_zero_tri.error().code == StatusCode::DegenerateGeometry, "Zero triangle code is DegenerateGeometry");

    // 7. Premature EOF before endsolid
    const std::string premature_eof = "solid test\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\n";
    const auto res_eof = parse_ascii_stl_string(premature_eof);
    GEO_ASSERT(res_eof.is_error(), "Premature EOF before endsolid returns error");
    GEO_ASSERT(res_eof.error().code == StatusCode::InvalidInput, "Premature EOF code is InvalidInput");
}

void test_csv_valid_parsing() {
    std::cout << "[TEST GROUP] ANSYS Temperature CSV Valid Parsing...\n";

    const auto res = parse_temperature_csv_file(resolve_sample_path("data/samples/valid_nodal_temperature.csv"));
    GEO_ASSERT(res.is_ok(), "Parsing valid_nodal_temperature.csv succeeds");

    if (res.is_ok()) {
        const auto& field = res.value();
        GEO_ASSERT(field.node_count() == 5, "Exactly 5 nodes parsed");

        // Verify specific nodes
        const auto* n101 = field.find_node(101);
        GEO_ASSERT(n101 != nullptr, "Node 101 found in field");
        if (n101) {
            GEO_ASSERT(validation::approx_equal(n101->position.x, 0.0) &&
                       validation::approx_equal(n101->position.y, 0.0) &&
                       validation::approx_equal(n101->position.z, 0.0), "Node 101 coordinates (0,0,0)");
            GEO_ASSERT(validation::approx_equal(n101->temperature_c, 100.0), "Node 101 temperature is 100.0 C");
        }

        const auto* n102 = field.find_node(102);
        GEO_ASSERT(n102 != nullptr, "Node 102 found in field");
        if (n102) {
            GEO_ASSERT(validation::approx_equal(n102->temperature_c, 185.0), "Node 102 temperature is 185.0 C (midpoint)");
        }

        const auto* n_missing = field.find_node(999);
        GEO_ASSERT(n_missing == nullptr, "Non-existent node 999 returns nullptr");

        // Temperature extrema and average
        GEO_ASSERT(validation::approx_equal(field.min_temperature, 20.0), "Minimum temperature is 20.0 C");
        GEO_ASSERT(validation::approx_equal(field.max_temperature, 185.0), "Maximum temperature is 185.0 C");
        // Sum = 100 + 185 + 20 + 75.5 + 45.2 = 425.7, avg = 425.7 / 5 = 85.14
        GEO_ASSERT(validation::approx_equal(field.average_temperature, 85.14), "Average temperature is 85.14 C");

        // Bounding box checks
        const auto& bbox = field.bounding_box;
        GEO_ASSERT(validation::approx_equal(bbox.min_point.x, 0.0) &&
                   validation::approx_equal(bbox.min_point.y, 0.0) &&
                   validation::approx_equal(bbox.min_point.z, 0.0), "Node bounding box min (0,0,0)");
        GEO_ASSERT(validation::approx_equal(bbox.max_point.x, 1.0) &&
                   validation::approx_equal(bbox.max_point.y, 0.5) &&
                   validation::approx_equal(bbox.max_point.z, 0.0), "Node bounding box max (1, 0.5, 0)");
    }
}

void test_csv_error_handling() {
    std::cout << "[TEST GROUP] ANSYS Temperature CSV Error Handling & Rejection...\n";

    // 1. Missing file
    const auto res_missing = parse_temperature_csv_file("data/samples/non_existent.csv");
    GEO_ASSERT(res_missing.is_error(), "Missing CSV returns error");
    GEO_ASSERT(res_missing.error().code == StatusCode::FileNotFound, "Missing CSV code is FileNotFound");

    // 2. Empty file (0 bytes)
    const auto res_empty = parse_temperature_csv_file(resolve_sample_path("data/samples/empty.csv"));
    GEO_ASSERT(res_empty.is_error(), "Empty CSV returns error");
    GEO_ASSERT(res_empty.error().code == StatusCode::InvalidInput, "Empty CSV code is InvalidInput");

    // 3. Header-only file (no data rows)
    const auto res_hdr_only = parse_temperature_csv_file(resolve_sample_path("data/samples/header_only.csv"));
    GEO_ASSERT(res_hdr_only.is_error(), "Header-only CSV returns error");
    GEO_ASSERT(res_hdr_only.error().code == StatusCode::InvalidInput, "Header-only CSV code is InvalidInput");

    // 4. Invalid header schema
    const auto res_bad_hdr = parse_temperature_csv_file(resolve_sample_path("data/samples/invalid_header.csv"));
    GEO_ASSERT(res_bad_hdr.is_error(), "Invalid header schema returns error");
    GEO_ASSERT(res_bad_hdr.error().code == StatusCode::InvalidInput, "Invalid header code is InvalidInput");

    // 5. Duplicate node_id
    const auto res_dup = parse_temperature_csv_file(resolve_sample_path("data/samples/duplicate_id.csv"));
    GEO_ASSERT(res_dup.is_error(), "Duplicate node_id returns error");
    GEO_ASSERT(res_dup.error().code == StatusCode::InvalidInput, "Duplicate node_id code is InvalidInput");

    // 6. Below absolute zero
    const auto res_abs_zero = parse_temperature_csv_file(resolve_sample_path("data/samples/below_absolute_zero.csv"));
    GEO_ASSERT(res_abs_zero.is_error(), "Temperature below absolute zero returns error");
    GEO_ASSERT(res_abs_zero.error().code == StatusCode::OutOfRange, "Below absolute zero code is OutOfRange");

    // 7. Non-finite coordinate row in memory
    const std::string nan_coord_csv = "node_id,x_m,y_m,z_m,temperature_C\n1,NaN,0.0,0.0,25.0\n";
    const auto res_nan_coord = parse_temperature_csv_string(nan_coord_csv);
    GEO_ASSERT(res_nan_coord.is_error(), "CSV with NaN coordinate returns error");
    GEO_ASSERT(res_nan_coord.error().code == StatusCode::InvalidInput, "NaN coordinate code is InvalidInput");

    // 8. Non-finite temperature in memory
    const std::string inf_temp_csv = "node_id,x_m,y_m,z_m,temperature_C\n1,0.0,0.0,0.0,Inf\n";
    const auto res_inf_temp = parse_temperature_csv_string(inf_temp_csv);
    GEO_ASSERT(res_inf_temp.is_error(), "CSV with Inf temperature returns error");
    GEO_ASSERT(res_inf_temp.error().code == StatusCode::InvalidInput, "Inf temperature code is InvalidInput");

    // 9. Incorrect column count (too few columns)
    const std::string few_cols_csv = "node_id,x_m,y_m,z_m,temperature_C\n1,0.0,0.0,25.0\n";
    const auto res_few = parse_temperature_csv_string(few_cols_csv);
    GEO_ASSERT(res_few.is_error(), "CSV row with 4 columns returns error");
    GEO_ASSERT(res_few.error().code == StatusCode::InvalidInput, "Few columns code is InvalidInput");

    // 10. Incorrect column count (too many columns)
    const std::string many_cols_csv = "node_id,x_m,y_m,z_m,temperature_C\n1,0.0,0.0,0.0,25.0,extra\n";
    const auto res_many = parse_temperature_csv_string(many_cols_csv);
    GEO_ASSERT(res_many.is_error(), "CSV row with 6 columns returns error");
    GEO_ASSERT(res_many.error().code == StatusCode::InvalidInput, "Many columns code is InvalidInput");
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  UNIFIED ENGINEERING WORKBENCH TESTS  \n";
    std::cout << "  Stage 4: STL Geometry & CSV Parsers \n";
    std::cout << "========================================\n\n";

    test_ascii_stl_valid_triangle();
    test_ascii_stl_bracket_mesh();
    test_ascii_stl_degenerate_detection();
    test_ascii_stl_error_handling();
    test_csv_valid_parsing();
    test_csv_error_handling();

    std::cout << "\n========================================\n";
    std::cout << "  GEOMETRY & CSV TESTS SUMMARY: " << g_geo_passed << " PASSED, " << g_geo_failed << " FAILED\n";
    std::cout << "========================================\n";

    return (g_geo_failed == 0) ? 0 : 1;
}
