#include "modules/geometry/csv_parser.h"

#include <fstream>
#include <sstream>
#include <unordered_set>
#include <algorithm>
#include <limits>
#include <cctype>

namespace workbench::geometry {

namespace {

// Trims whitespace and carriage returns
std::string trim(const std::string& str) {
    const auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Splits a CSV line by comma
std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream token_stream(line);
    while (std::getline(token_stream, token, ',')) {
        tokens.push_back(trim(token));
    }
    // If line ended with a comma, push an empty token
    if (!line.empty() && line.back() == ',') {
        tokens.push_back("");
    }
    return tokens;
}

// Parses string into double and checks finiteness
bool parse_double(const std::string& str, double& out_val) {
    if (str.empty()) {
        return false;
    }
    try {
        size_t idx = 0;
        const double val = std::stod(str, &idx);
        if (idx != str.size() || !std::isfinite(val)) {
            return false;
        }
        out_val = val;
        return true;
    } catch (...) {
        return false;
    }
}

// Parses string into int64_t
bool parse_int64(const std::string& str, int64_t& out_val) {
    if (str.empty()) {
        return false;
    }
    try {
        size_t idx = 0;
        const long long val = std::stoll(str, &idx);
        if (idx != str.size()) {
            return false;
        }
        out_val = static_cast<int64_t>(val);
        return true;
    } catch (...) {
        return false;
    }
}

core::Result<NodalTemperatureField> parse_csv_stream(std::istream& is, const std::string& source_label) {
    std::string line;
    size_t line_number = 0;
    bool found_header = false;

    NodalTemperatureField field;
    std::unordered_set<int64_t> seen_node_ids;

    double temp_sum = 0.0;
    field.min_temperature = std::numeric_limits<double>::infinity();
    field.max_temperature = -std::numeric_limits<double>::infinity();

    const std::string expected_header = "node_id,x_m,y_m,z_m,temperature_C";

    while (std::getline(is, line)) {
        line_number++;
        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue; // Skip empty lines
        }

        if (!found_header) {
            // Validate exact header format
            if (trimmed != expected_header) {
                return core::Result<NodalTemperatureField>::failure(
                    core::StatusCode::InvalidInput,
                    "Invalid CSV header in " + source_label + " at line " + std::to_string(line_number) +
                    ". Expected: '" + expected_header + "', found: '" + trimmed + "'."
                );
            }
            found_header = true;
            continue;
        }

        // Parse data rows
        const auto tokens = split_csv_line(trimmed);
        if (tokens.size() != 5) {
            return core::Result<NodalTemperatureField>::failure(
                core::StatusCode::InvalidInput,
                "Malformed CSV row in " + source_label + " at line " + std::to_string(line_number) +
                ": expected 5 columns, found " + std::to_string(tokens.size()) + "."
            );
        }

        // 1. Parse node_id
        int64_t node_id = 0;
        if (!parse_int64(tokens[0], node_id)) {
            return core::Result<NodalTemperatureField>::failure(
                core::StatusCode::InvalidInput,
                "Invalid node_id in " + source_label + " at line " + std::to_string(line_number) +
                ": '" + tokens[0] + "' is not a valid integer."
            );
        }

        // Duplicate node_id check
        if (seen_node_ids.find(node_id) != seen_node_ids.end()) {
            return core::Result<NodalTemperatureField>::failure(
                core::StatusCode::InvalidInput,
                "Duplicate node_id detected in " + source_label + " at line " + std::to_string(line_number) +
                ": node_id " + std::to_string(node_id) + " already exists."
            );
        }
        seen_node_ids.insert(node_id);

        // 2. Parse coordinates (x_m, y_m, z_m)
        double x = 0.0, y = 0.0, z = 0.0;
        if (!parse_double(tokens[1], x) || !parse_double(tokens[2], y) || !parse_double(tokens[3], z)) {
            return core::Result<NodalTemperatureField>::failure(
                core::StatusCode::InvalidInput,
                "Non-finite or invalid coordinate in " + source_label + " at line " + std::to_string(line_number) +
                "."
            );
        }

        // 3. Parse temperature_C
        double temp_c = 0.0;
        if (!parse_double(tokens[4], temp_c)) {
            return core::Result<NodalTemperatureField>::failure(
                core::StatusCode::InvalidInput,
                "Non-finite or invalid temperature in " + source_label + " at line " + std::to_string(line_number) +
                "."
            );
        }

        // Validate physical bounds of temperature
        if (!core::validation::is_physically_valid_temperature_celsius(temp_c)) {
            return core::Result<NodalTemperatureField>::failure(
                core::StatusCode::OutOfRange,
                "Temperature value out of range in " + source_label + " at line " + std::to_string(line_number) +
                ": " + std::to_string(temp_c) + " C is below absolute zero (-273.15 C)."
            );
        }

        TemperatureNode node;
        node.node_id = node_id;
        node.position = core::Vector3D{x, y, z};
        node.temperature_c = temp_c;

        field.nodes.push_back(node);
        field.bounding_box.include_point(node.position);

        temp_sum += temp_c;
        field.min_temperature = std::min(field.min_temperature, temp_c);
        field.max_temperature = std::max(field.max_temperature, temp_c);
    }

    if (!found_header) {
        return core::Result<NodalTemperatureField>::failure(
            core::StatusCode::InvalidInput,
            "CSV file in " + source_label + " is empty or contains no header."
        );
    }

    if (field.nodes.empty()) {
        return core::Result<NodalTemperatureField>::failure(
            core::StatusCode::InvalidInput,
            "CSV file in " + source_label + " contains a valid header but zero data rows."
        );
    }

    field.average_temperature = temp_sum / static_cast<double>(field.nodes.size());

    return core::Result<NodalTemperatureField>::success(std::move(field));
}

} // anonymous namespace

const TemperatureNode* NodalTemperatureField::find_node(int64_t id) const noexcept {
    for (const auto& node : nodes) {
        if (node.node_id == id) {
            return &node;
        }
    }
    return nullptr;
}

core::Result<NodalTemperatureField> parse_temperature_csv_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return core::Result<NodalTemperatureField>::failure(
            core::StatusCode::FileNotFound,
            "Cannot open CSV file: " + filepath
        );
    }

    // Check if file is completely empty (0 bytes)
    file.seekg(0, std::ios::end);
    const auto file_size = file.tellg();
    if (file_size <= 0) {
        return core::Result<NodalTemperatureField>::failure(
            core::StatusCode::InvalidInput,
            "CSV file is empty (0 bytes): " + filepath
        );
    }
    file.seekg(0, std::ios::beg);

    return parse_csv_stream(file, filepath);
}

core::Result<NodalTemperatureField> parse_temperature_csv_string(const std::string& content) {
    if (content.empty()) {
        return core::Result<NodalTemperatureField>::failure(
            core::StatusCode::InvalidInput,
            "CSV string content is empty."
        );
    }
    std::istringstream iss(content);
    return parse_csv_stream(iss, "in-memory-string");
}

} // namespace workbench::geometry
