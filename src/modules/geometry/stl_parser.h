#pragma once

#include "core/vector3.h"
#include "core/bounding_box.h"
#include "core/result.h"
#include "core/validation.h"

#include <string>
#include <vector>
#include <cstddef>

namespace workbench::geometry {

/**
 * @brief Represents a single triangular facet extracted from an STL file.
 */
struct Triangle {
    core::Vector3D normal;
    core::Vector3D v1;
    core::Vector3D v2;
    core::Vector3D v3;
    bool is_degenerate = false;

    /**
     * @brief Computes the surface area of this triangular facet.
     * Area = 0.5 * ||(v2 - v1) x (v3 - v1)||
     */
    double area() const noexcept {
        const core::Vector3D e1 = v2 - v1;
        const core::Vector3D e2 = v3 - v1;
        return 0.5 * e1.cross(e2).magnitude();
    }
};

/**
 * @brief Represents a complete 3D surface mesh parsed from an ASCII STL dataset.
 */
struct StlMesh {
    std::string solid_name;
    std::vector<Triangle> triangles;
    core::BoundingBox3D bounding_box;
    size_t degenerate_count = 0;
    double total_surface_area = 0.0;

    size_t triangle_count() const noexcept {
        return triangles.size();
    }
};

/**
 * @brief Parses an ASCII STL file from disk.
 * 
 * @param filepath Full path to the .stl file on disk.
 * @return core::Result<StlMesh> The parsed mesh on success, or a descriptive Error.
 */
core::Result<StlMesh> parse_ascii_stl_file(const std::string& filepath);

/**
 * @brief Parses an ASCII STL dataset from an in-memory string.
 * 
 * @param content The raw text content of the ASCII STL.
 * @return core::Result<StlMesh> The parsed mesh on success, or a descriptive Error.
 */
core::Result<StlMesh> parse_ascii_stl_string(const std::string& content);

} // namespace workbench::geometry
