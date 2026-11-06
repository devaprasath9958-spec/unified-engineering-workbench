#include "modules/geometry/stl_parser.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace workbench::geometry {

namespace {

// Trims leading and trailing whitespace from a string view or string
std::string trim(const std::string& str) {
    const auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Case-insensitive prefix comparison
bool starts_with_case_insensitive(const std::string& str, const std::string& prefix) {
    if (str.size() < prefix.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(str[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

// Parses three coordinates from a stream (e.g. for normal or vertex)
bool parse_three_doubles(std::istringstream& iss, core::Vector3D& out_vec) {
    double x = 0.0, y = 0.0, z = 0.0;
    if (!(iss >> x >> y >> z)) {
        return false;
    }
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        return false;
    }
    out_vec.x = x;
    out_vec.y = y;
    out_vec.z = z;
    return true;
}

// Internal line-by-line parser consuming any std::istream
core::Result<StlMesh> parse_ascii_stl_stream(std::istream& is, const std::string& source_label) {
    std::string line;
    size_t line_number = 0;
    bool found_solid = false;
    bool found_endsolid = false;

    StlMesh mesh;

    // FSM tracking current facet parsing
    bool in_facet = false;
    bool in_loop = false;
    core::Vector3D current_normal;
    std::vector<core::Vector3D> current_vertices;

    while (std::getline(is, line)) {
        line_number++;
        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue; // Skip empty lines
        }

        std::istringstream iss(trimmed);
        std::string token;
        iss >> token;

        // Convert token to lower case for case-insensitive keyword matching
        std::string lower_token = token;
        std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (!found_solid) {
            if (lower_token == "solid") {
                found_solid = true;
                // Remaining text on line is the solid name
                std::string name;
                std::getline(iss, name);
                mesh.solid_name = trim(name);
                continue;
            } else {
                return core::Result<StlMesh>::failure(
                    core::StatusCode::InvalidInput,
                    "Malformed STL in " + source_label + " at line " + std::to_string(line_number) +
                    ": expected 'solid' header, found '" + token + "'."
                );
            }
        }

        if (lower_token == "facet") {
            if (in_facet) {
                return core::Result<StlMesh>::failure(
                    core::StatusCode::InvalidInput,
                    "Malformed STL at line " + std::to_string(line_number) +
                    ": unexpected nested 'facet' definition."
                );
            }
            std::string sub_token;
            iss >> sub_token;
            std::transform(sub_token.begin(), sub_token.end(), sub_token.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (sub_token != "normal") {
                return core::Result<StlMesh>::failure(
                    core::StatusCode::InvalidInput,
                    "Malformed STL at line " + std::to_string(line_number) +
                    ": expected 'normal' after 'facet'."
                );
            }

            if (!parse_three_doubles(iss, current_normal)) {
                return core::Result<StlMesh>::failure(
                    core::StatusCode::InvalidInput,
                    "Malformed STL at line " + std::to_string(line_number) +
                    ": invalid or non-finite normal coordinates."
                );
            }
            in_facet = true;
            current_vertices.clear();
        }
        else if (lower_token == "outer") {
            if (!in_facet || in_loop) {
                return core::Result<StlMesh>::failure(
                    core::StatusCode::InvalidInput,
                    "Malformed STL at line " + std::to_string(line_number) +
                    ": unexpected 'outer' keyword outside or nested within loop."
                );
            }
            std::string sub_token;
            iss >> sub_token;
            std::transform(sub_token.begin(), sub_token.end(), sub_token.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (sub_token != "loop") {
                return core::Result<StlMesh>::failure(
                    core::StatusCode::InvalidInput,
                    "Malformed STL at line " + std::to_string(line_number) +
                    ": expected 'loop' after 'outer'."
                );
            }
            in_loop = true;
            current_vertices.clear();
        }
        else if (lower_token == "vertex") {
            if (!in_loop) {
                return core::Result<StlMesh>::failure(
                    core::StatusCode::InvalidInput,
                    "Malformed STL at line " + std::to_string(line_number) +
                    ": 'vertex' found outside an 'outer loop' block."
                );
            }
            core::Vector3D v;
            if (!parse_three_doubles(iss, v)) {
                return core::Result<StlMesh>::failure(
                    core::StatusCode::InvalidInput,
                    "Malformed STL at line " + std::to_string(line_number) +
                    ": invalid or non-finite vertex coordinates."
                );
            }
            current_vertices.push_back(v);
        }
        else if (lower_token == "endloop") {
            if (!in_loop) {
                return core::Result<StlMesh>::failure(
                    core::StatusCode::InvalidInput,
                    "Malformed STL at line " + std::to_string(line_number) +
                    ": unexpected 'endloop' without matching 'outer loop'."
                );
            }
            if (current_vertices.size() != 3) {
                return core::Result<StlMesh>::failure(
                    core::StatusCode::InvalidInput,
                    "Malformed STL at line " + std::to_string(line_number) +
                    ": triangular facet must have exactly 3 vertices, found " +
                    std::to_string(current_vertices.size()) + "."
                );
            }
            in_loop = false;
        }
        else if (lower_token == "endfacet") {
            if (!in_facet || in_loop) {
                return core::Result<StlMesh>::failure(
                    core::StatusCode::InvalidInput,
                    "Malformed STL at line " + std::to_string(line_number) +
                    ": unexpected 'endfacet' before loop completion."
                );
            }
            if (current_vertices.size() != 3) {
                return core::Result<StlMesh>::failure(
                    core::StatusCode::InvalidInput,
                    "Malformed STL at line " + std::to_string(line_number) +
                    ": facet ended with invalid vertex count."
                );
            }

            Triangle tri;
            tri.normal = current_normal;
            tri.v1 = current_vertices[0];
            tri.v2 = current_vertices[1];
            tri.v3 = current_vertices[2];

            // Expand bounding box with each vertex
            mesh.bounding_box.include_point(tri.v1);
            mesh.bounding_box.include_point(tri.v2);
            mesh.bounding_box.include_point(tri.v3);

            mesh.triangles.push_back(tri);
            in_facet = false;
        }
        else if (lower_token == "endsolid") {
            found_endsolid = true;
            break;
        }
    }

    if (!found_solid) {
        return core::Result<StlMesh>::failure(
            core::StatusCode::InvalidInput,
            "Empty or unreadable STL data in " + source_label + ": no 'solid' header found."
        );
    }

    if (!found_endsolid) {
        return core::Result<StlMesh>::failure(
            core::StatusCode::InvalidInput,
            "Malformed STL in " + source_label + ": unexpected end of file before 'endsolid' statement."
        );
    }

    if (mesh.triangles.empty()) {
        return core::Result<StlMesh>::failure(
            core::StatusCode::DegenerateGeometry,
            "STL model contains zero triangular facets."
        );
    }

    // Post-processing: bounding box diagonal and scale-relative degeneracy check
    const double diag = mesh.bounding_box.diagonal_length();
    mesh.degenerate_count = 0;
    mesh.total_surface_area = 0.0;

    for (auto& tri : mesh.triangles) {
        const double area = tri.area();
        mesh.total_surface_area += area;

        // Degeneracy evaluation using scale-relative tolerance
        tri.is_degenerate = core::validation::is_triangle_degenerate(tri.v1, tri.v2, tri.v3, diag);
        if (tri.is_degenerate) {
            mesh.degenerate_count++;
        }
    }

    return core::Result<StlMesh>::success(std::move(mesh));
}

} // anonymous namespace

core::Result<StlMesh> parse_ascii_stl_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return core::Result<StlMesh>::failure(
            core::StatusCode::FileNotFound,
            "Cannot open STL file: " + filepath
        );
    }

    // Check if file is completely empty (0 bytes)
    file.seekg(0, std::ios::end);
    const auto file_size = file.tellg();
    if (file_size <= 0) {
        return core::Result<StlMesh>::failure(
            core::StatusCode::InvalidInput,
            "STL file is empty (0 bytes): " + filepath
        );
    }
    file.seekg(0, std::ios::beg);

    return parse_ascii_stl_stream(file, filepath);
}

core::Result<StlMesh> parse_ascii_stl_string(const std::string& content) {
    if (content.empty()) {
        return core::Result<StlMesh>::failure(
            core::StatusCode::InvalidInput,
            "STL string content is empty."
        );
    }
    std::istringstream iss(content);
    return parse_ascii_stl_stream(iss, "in-memory-string");
}

} // namespace workbench::geometry
