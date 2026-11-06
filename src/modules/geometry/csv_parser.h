#pragma once

#include "core/vector3.h"
#include "core/bounding_box.h"
#include "core/result.h"
#include "core/validation.h"

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace workbench::geometry {

/**
 * @brief Represents a single nodal temperature data point.
 */
struct TemperatureNode {
    int64_t node_id = 0;
    core::Vector3D position;
    double temperature_c = 0.0;
};

/**
 * @brief Represents a full set of temperature nodes imported from an ANSYS CSV file.
 */
struct NodalTemperatureField {
    std::vector<TemperatureNode> nodes;
    core::BoundingBox3D bounding_box;
    double min_temperature = 0.0;
    double max_temperature = 0.0;
    double average_temperature = 0.0;

    size_t node_count() const noexcept {
        return nodes.size();
    }

    /**
     * @brief Finds a node by its unique node_id.
     * @return const TemperatureNode* Pointer to node if found, nullptr otherwise.
     */
    const TemperatureNode* find_node(int64_t id) const noexcept;
};

/**
 * @brief Parses an ANSYS nodal temperature CSV file from disk.
 * 
 * Validates header schema: node_id,x_m,y_m,z_m,temperature_C
 * Checks for duplicate IDs, non-finite values, and temperatures below absolute zero.
 * 
 * @param filepath Path to the CSV file.
 * @return core::Result<NodalTemperatureField> Parsed temperature field or Error.
 */
core::Result<NodalTemperatureField> parse_temperature_csv_file(const std::string& filepath);

/**
 * @brief Parses an ANSYS nodal temperature CSV dataset from an in-memory string.
 * 
 * @param content CSV string content.
 * @return core::Result<NodalTemperatureField> Parsed temperature field or Error.
 */
core::Result<NodalTemperatureField> parse_temperature_csv_string(const std::string& content);

} // namespace workbench::geometry
