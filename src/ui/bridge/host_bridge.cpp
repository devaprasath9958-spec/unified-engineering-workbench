#include "ui/bridge/host_bridge.h"
#include "modules/geometry/stl_parser.h"
#include "modules/geometry/csv_parser.h"
#include "modules/thermal/fem_1d_solver.h"
#include "modules/performance/benchmark_runner.h"
#include "modules/performance/system_info.h"
#include "modules/hardware/hardware_manager.h"
#include "modules/hardware/hardware_types.h"
#include "modules/backprop/backprop_engine.h"
#include "modules/backprop/network_types.h"
#include "core/validation.h"

#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <cmath>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace workbench::ui {

namespace {

// Robust JSON string extractor
std::string extract_json_field(const std::string& json, const std::string& field_name) {
    const std::string key = "\"" + field_name + "\"";
    size_t key_pos = json.find(key);
    if (key_pos == std::string::npos) {
        return "";
    }

    size_t colon_pos = json.find(':', key_pos + key.size());
    if (colon_pos == std::string::npos) {
        return "";
    }

    size_t val_start = json.find_first_not_of(" \t\r\n", colon_pos + 1);
    if (val_start == std::string::npos) {
        return "";
    }

    if (json[val_start] == '\"') {
        size_t val_end = json.find('\"', val_start + 1);
        if (val_end != std::string::npos) {
            return json.substr(val_start + 1, val_end - val_start - 1);
        }
    } else if (json[val_start] == '{') {
        int depth = 0;
        for (size_t i = val_start; i < json.size(); ++i) {
            if (json[i] == '{') depth++;
            else if (json[i] == '}') {
                depth--;
                if (depth == 0) {
                    return json.substr(val_start, i - val_start + 1);
                }
            }
        }
    } else if (json[val_start] == '[') {
        int depth = 0;
        for (size_t i = val_start; i < json.size(); ++i) {
            if (json[i] == '[') depth++;
            else if (json[i] == ']') {
                depth--;
                if (depth == 0) {
                    return json.substr(val_start, i - val_start + 1);
                }
            }
        }
    } else {
        size_t val_end = json.find_first_of(",}\r\n", val_start);
        if (val_end != std::string::npos) {
            return json.substr(val_start, val_end - val_start);
        } else {
            return json.substr(val_start);
        }
    }
    return "";
}

double extract_json_double(const std::string& json, const std::string& field_name, double default_val = 0.0) {
    const std::string s = extract_json_field(json, field_name);
    if (s.empty()) return default_val;
    try {
        return std::stod(s);
    } catch (...) {
        return default_val;
    }
}

int64_t extract_json_int(const std::string& json, const std::string& field_name, int64_t default_val = 0) {
    const std::string s = extract_json_field(json, field_name);
    if (s.empty()) return default_val;
    try {
        return std::stoll(s);
    } catch (...) {
        return default_val;
    }
}

bool extract_json_bool(const std::string& json, const std::string& field_name, bool default_val = false) {
    const std::string s = extract_json_field(json, field_name);
    if (s == "true" || s == "1") return true;
    if (s == "false" || s == "0") return false;
    return default_val;
}

std::vector<size_t> extract_json_size_t_array(const std::string& json, const std::string& field_name) {
    std::vector<size_t> result;
    const std::string arr_str = extract_json_field(json, field_name);
    if (arr_str.empty() || arr_str.front() != '[' || arr_str.back() != ']') {
        return result;
    }
    std::string inner = arr_str.substr(1, arr_str.size() - 2);
    std::stringstream ss(inner);
    std::string item;
    while (std::getline(ss, item, ',')) {
        size_t start = item.find_first_not_of(" \t\r\n");
        size_t end = item.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            std::string clean = item.substr(start, end - start + 1);
            try {
                result.push_back(static_cast<size_t>(std::stoull(clean)));
            } catch (...) {}
        }
    }
    return result;
}

// Escape string for reliable JSON
std::string escape_json(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) <= 0x1f) {
                    o << "\\u00";
                    char buf[3];
                    snprintf(buf, sizeof(buf), "%02x", static_cast<unsigned char>(c));
                    o << buf;
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}

int64_t current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // namespace

std::string BridgeResponse::to_json() const {
    std::ostringstream ss;
    ss << "{";
    if (!request_id.empty()) {
        ss << "\"request_id\":\"" << escape_json(request_id) << "\",";
    }
    ss << "\"success\":" << (success ? "true" : "false") << ","
       << "\"action\":\"" << escape_json(action) << "\","
       << "\"timestamp_ms\":" << current_time_ms() << ",";
    
    if (success) {
        if (!payload_json.empty() && (payload_json.front() == '{' || payload_json.front() == '[')) {
            ss << "\"payload\":" << payload_json;
        } else {
            ss << "\"payload\":\"" << escape_json(payload_json) << "\"";
        }
    } else {
        ss << "\"error\":\"" << escape_json(error_message) << "\"";
        if (!payload_json.empty()) {
            if (payload_json.front() == '{' || payload_json.front() == '[') {
                ss << ",\"payload\":" << payload_json;
            } else {
                ss << ",\"payload\":\"" << escape_json(payload_json) << "\"";
            }
        }
    }
    ss << "}";
    return ss.str();
}

HostBridge::HostBridge() {
    loaded_modules_ = {
        "Core Validation",
        "Geometry & CSV Parsers (Stage 4)",
        "Thermal FEM 1D Solver (Stage 5)",
        "Multicore Benchmark Engine (Stage 6)",
        "Hardware Integration Mock Adapters (Stage 7)",
        "Backpropagation & Numerical Verification (Stage 8)"
    };
    hardware_manager_ = hardware::create_default_hardware_manager();
}

void HostBridge::set_webview_runtime_version(const std::string& version) {
    webview_runtime_version_ = version;
}

void HostBridge::set_compiler_info(const std::string& info) {
    compiler_info_ = info;
}

bool HostBridge::is_origin_allowed(const std::string& origin) {
    if (origin.empty() || origin == "null") {
        return true;
    }
    if (origin.rfind("file://", 0) == 0) {
        return true;
    }
    if (origin.rfind("https://workbench.local", 0) == 0 || origin.rfind("http://workbench.local", 0) == 0) {
        return true;
    }
    return false;
}

bool HostBridge::is_safe_relative_export_path(const std::string& path) {
    if (path.empty()) return false;
    // Disallow directory traversal
    if (path.find("..") != std::string::npos) return false;
    // Disallow absolute paths (drive letters or leading root slashes)
    if (path.find(':') != std::string::npos || path.front() == '/' || path.front() == '\\') return false;
    // Enforce that path must reside within results/
    if (path.rfind("results/", 0) != 0 && path.rfind("results\\", 0) != 0) return false;
    // Must end with .csv
    if (path.size() < 4 || path.substr(path.size() - 4) != ".csv") return false;
    return true;
}

HostSystemStatus HostBridge::get_status() const {
    HostSystemStatus status;
    status.compiler_info = compiler_info_;
    status.webview_runtime_version = webview_runtime_version_;
    status.loaded_modules = loaded_modules_;
    status.backend_ready = true;
    status.timestamp_ms = current_time_ms();
    return status;
}

std::string HostBridge::process_message(const std::string& raw_message, const std::string& origin) {
    // 1. Origin verification
    if (!is_origin_allowed(origin)) {
        BridgeResponse resp;
        resp.success = false;
        resp.action = "security_error";
        resp.error_message = "Rejected: Origin '" + origin + "' is not permitted.";
        return resp.to_json();
    }

    // 2. Message size limit verification (64 KB incoming limit)
    if (raw_message.size() > kMaxMessageSizeBytes) {
        BridgeResponse resp;
        resp.success = false;
        resp.action = "security_error";
        resp.error_message = "Rejected: Message size exceeds 64 KB limit.";
        return resp.to_json();
    }

    // 3. Extract request_id and action
    const std::string request_id = extract_json_field(raw_message, "request_id");
    const std::string action = extract_json_field(raw_message, "action");
    if (action.empty()) {
        BridgeResponse resp;
        resp.request_id = request_id;
        resp.success = false;
        resp.action = "invalid_format";
        resp.error_message = "Rejected: Missing or empty 'action' field in JSON.";
        return resp.to_json();
    }

    const std::string payload = extract_json_field(raw_message, "payload");

    // 4. Strict action allowlist dispatch
    BridgeResponse resp;
    if (action == "ping") {
        resp = handle_ping(request_id, payload);
    } else if (action == "handshake") {
        resp = handle_handshake(request_id, payload);
    } else if (action == "get_status") {
        resp = handle_get_status(request_id, payload);
    }
    // Module 1: Geometry
    else if (action == "geometry_load_sample") {
        resp = handle_geometry_load_sample(request_id, payload);
    } else if (action == "geometry_parse_custom") {
        resp = handle_geometry_parse_custom(request_id, payload);
    }
    // Module 2: Thermal FEM
    else if (action == "thermal_solve") {
        resp = handle_thermal_solve(request_id, payload);
    } else if (action == "thermal_export_csv") {
        resp = handle_thermal_export_csv(request_id, payload);
    }
    // Module 3: Multicore Performance
    else if (action == "benchmark_get_hardware_info") {
        resp = handle_benchmark_get_hardware_info(request_id, payload);
    } else if (action == "benchmark_run") {
        resp = handle_benchmark_run(request_id, payload);
    } else if (action == "benchmark_export_csv") {
        resp = handle_benchmark_export_csv(request_id, payload);
    }
    // Module 4: Hardware Integration
    else if (action == "hardware_discover") {
        resp = handle_hardware_discover(request_id, payload);
    } else if (action == "hardware_connect") {
        resp = handle_hardware_connect(request_id, payload);
    } else if (action == "hardware_disconnect") {
        resp = handle_hardware_disconnect(request_id, payload);
    } else if (action == "hardware_read_telemetry") {
        resp = handle_hardware_read_telemetry(request_id, payload);
    } else if (action == "hardware_send_command") {
        resp = handle_hardware_send_command(request_id, payload);
    } else if (action == "hardware_reset_simulation_flags") {
        resp = handle_hardware_reset_simulation_flags(request_id, payload);
    }
    // Module 5: Backpropagation
    else if (action == "backprop_verify_gradients") {
        resp = handle_backprop_verify_gradients(request_id, payload);
    } else if (action == "backprop_train") {
        resp = handle_backprop_train(request_id, payload);
    } else if (action == "backprop_step") {
        resp = handle_backprop_step(request_id, payload);
    } else if (action == "backprop_export_csv") {
        resp = handle_backprop_export_csv(request_id, payload);
    }
    // Native File Dialogs
    else if (action == "dialog_open_file") {
        resp = handle_dialog_open_file(request_id, payload);
    } else if (action == "dialog_save_file") {
        resp = handle_dialog_save_file(request_id, payload);
    } else {
        resp.request_id = request_id;
        resp.success = false;
        resp.action = action;
        resp.error_message = "Rejected: Action '" + action + "' is not permitted or unrecognized.";
    }

    std::string response_json = resp.to_json();

    // 5. Outgoing response safe limit verification (4 MB safe ceiling)
    if (response_json.size() > kMaxOutgoingMessageSizeBytes) {
        BridgeResponse overflow_resp;
        overflow_resp.request_id = request_id;
        overflow_resp.success = false;
        overflow_resp.action = action;
        overflow_resp.error_message = "Response payload exceeds safe maximum limit of 4 MB.";
        return overflow_resp.to_json();
    }

    return response_json;
}

BridgeResponse HostBridge::handle_ping(const std::string& req_id, const std::string& /*payload*/) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.success = true;
    resp.action = "pong";
    resp.payload_json = "{\"status\":\"alive\"}";
    return resp;
}

BridgeResponse HostBridge::handle_handshake(const std::string& req_id, const std::string& /*payload*/) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.success = true;
    resp.action = "handshake_ack";

    const HostSystemStatus status = get_status();
    std::ostringstream ss;
    ss << "{"
       << "\"app_name\":\"" << escape_json(status.app_name) << "\","
       << "\"app_version\":\"" << escape_json(status.app_version) << "\","
       << "\"host_platform\":\"" << escape_json(status.host_platform) << "\","
       << "\"cpp_standard\":\"" << escape_json(status.cpp_standard) << "\","
       << "\"compiler_info\":\"" << escape_json(status.compiler_info) << "\","
       << "\"webview_runtime\":\"" << escape_json(status.webview_runtime_version) << "\","
       << "\"backend_ready\":" << (status.backend_ready ? "true" : "false") << ","
       << "\"modules_count\":" << status.loaded_modules.size()
       << "}";
    resp.payload_json = ss.str();
    return resp;
}

BridgeResponse HostBridge::handle_get_status(const std::string& req_id, const std::string& /*payload*/) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.success = true;
    resp.action = "status_report";

    const HostSystemStatus status = get_status();
    std::ostringstream ss;
    ss << "{"
       << "\"app_name\":\"" << escape_json(status.app_name) << "\","
       << "\"app_version\":\"" << escape_json(status.app_version) << "\","
       << "\"host_platform\":\"" << escape_json(status.host_platform) << "\","
       << "\"cpp_standard\":\"" << escape_json(status.cpp_standard) << "\","
       << "\"compiler_info\":\"" << escape_json(status.compiler_info) << "\","
       << "\"webview_runtime\":\"" << escape_json(status.webview_runtime_version) << "\","
       << "\"backend_ready\":" << (status.backend_ready ? "true" : "false") << ","
       << "\"modules\":[";
    
    for (size_t i = 0; i < status.loaded_modules.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "\"" << escape_json(status.loaded_modules[i]) << "\"";
    }
    ss << "]}";

    resp.payload_json = ss.str();
    return resp;
}

// =============================================================================
// MODULE 1: GEOMETRY
// =============================================================================

static std::string resolve_existing_sample_path(const std::string& rel_path) {
    if (std::filesystem::exists(rel_path)) {
        return rel_path;
    }
    // Check relative to current executable directory
    wchar_t exe_buf[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exe_buf, MAX_PATH) > 0) {
        auto exe_dir = std::filesystem::path(exe_buf).parent_path();
        if (std::filesystem::exists(exe_dir / rel_path)) {
            return (exe_dir / rel_path).string();
        }
        if (std::filesystem::exists(exe_dir / ".." / rel_path)) {
            return (exe_dir / ".." / rel_path).string();
        }
        if (std::filesystem::exists(exe_dir / ".." / ".." / rel_path)) {
            return (exe_dir / ".." / ".." / rel_path).string();
        }
    }
    // Check parent directories of CWD
    if (std::filesystem::exists("../" + rel_path)) {
        return "../" + rel_path;
    }
    if (std::filesystem::exists("../../" + rel_path)) {
        return "../../" + rel_path;
    }
    return rel_path;
}

BridgeResponse HostBridge::handle_geometry_load_sample(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "geometry_load_sample_result";

    std::string key = extract_json_field(payload, "sample_key");
    if (key.empty()) {
        key = extract_json_field(payload, "sample_stl");
    }
    if (key.empty()) {
        key = extract_json_field(payload, "sample_csv");
    }

    std::string filepath;
    bool is_stl = true;

    if (key == "bracket" || key == "valid_bracket.stl") {
        filepath = "data/samples/valid_bracket.stl";
    } else if (key == "triangle" || key == "valid_triangle.stl") {
        filepath = "data/samples/valid_triangle.stl";
    } else if (key == "degenerate" || key == "degenerate_facet.stl") {
        filepath = "data/samples/degenerate_facet.stl";
    } else if (key == "nodal_temp" || key == "valid_nodal_temperature.csv") {
        filepath = "data/samples/valid_nodal_temperature.csv";
        is_stl = false;
    } else if (key == "thermal_n10" || key == "thermal_results_N10.csv") {
        filepath = "data/samples/thermal_results_N10.csv";
        is_stl = false;
    } else {
        resp.success = false;
        resp.error_message = "Invalid or unrecognized sample key: '" + key + "'.";
        return resp;
    }

    filepath = resolve_existing_sample_path(filepath);

    if (is_stl) {
        auto res = geometry::parse_ascii_stl_file(filepath);
        if (res.is_error()) {
            resp.success = false;
            resp.error_message = "Failed to parse STL sample: " + res.error().message;
            return resp;
        }

        const auto& mesh = res.value();
        const size_t original_count = mesh.triangle_count();
        bool simplified = false;
        std::string simplification_label;
        size_t stride = 1;

        // Enforce 25,000-facet STL limit without dropping geometry silently
        if (original_count > kMaxStlFacetsLimit) {
            stride = (original_count + kMaxStlFacetsLimit - 1) / kMaxStlFacetsLimit;
            simplified = true;
            simplification_label = "Simplified Visualization: Rendered every " + std::to_string(stride) +
                                   "th facet (" + std::to_string((original_count + stride - 1) / stride) +
                                   " of " + std::to_string(original_count) + " facets) to respect 25,000 facet limit.";
        }

        std::ostringstream ss;
        ss << std::setprecision(8);
        ss << "{"
           << "\"type\":\"stl\","
           << "\"solid_name\":\"" << escape_json(mesh.solid_name) << "\","
           << "\"total_triangles\":" << original_count << ","
           << "\"degenerate_count\":" << mesh.degenerate_count << ","
           << "\"total_surface_area\":" << mesh.total_surface_area << ","
           << "\"simplified_visualization\":" << (simplified ? "true" : "false") << ","
           << "\"simplification_label\":\"" << escape_json(simplification_label) << "\","
           << "\"bounding_box\":{"
           << "\"min\":[" << mesh.bounding_box.min_point.x << "," << mesh.bounding_box.min_point.y << "," << mesh.bounding_box.min_point.z << "],"
           << "\"max\":[" << mesh.bounding_box.max_point.x << "," << mesh.bounding_box.max_point.y << "," << mesh.bounding_box.max_point.z << "],"
           << "\"span\":[" << mesh.bounding_box.dimensions().x << "," << mesh.bounding_box.dimensions().y << "," << mesh.bounding_box.dimensions().z << "]"
           << "},"
           << "\"vertices\":[";

        size_t rendered_count = 0;
        for (size_t i = 0; i < original_count; i += stride) {
            const auto& t = mesh.triangles[i];
            if (rendered_count > 0) ss << ",";
            ss << t.v1.x << "," << t.v1.y << "," << t.v1.z << ","
               << t.v2.x << "," << t.v2.y << "," << t.v2.z << ","
               << t.v3.x << "," << t.v3.y << "," << t.v3.z;
            rendered_count++;
        }
        ss << "],"
           << "\"normals\":[";
        rendered_count = 0;
        for (size_t i = 0; i < original_count; i += stride) {
            const auto& t = mesh.triangles[i];
            for (int v = 0; v < 3; ++v) {
                if (rendered_count > 0) ss << ",";
                ss << t.normal.x << "," << t.normal.y << "," << t.normal.z;
                rendered_count++;
            }
        }
        ss << "],"
           << "\"rendered_triangles\":" << (rendered_count / 3)
           << "}";

        resp.success = true;
        resp.payload_json = ss.str();
    } else {
        auto res = geometry::parse_temperature_csv_file(filepath);
        if (res.is_error()) {
            resp.success = false;
            resp.error_message = "Failed to parse CSV sample: " + res.error().message;
            return resp;
        }

        const auto& field = res.value();
        std::ostringstream ss;
        ss << std::setprecision(8);
        ss << "{"
           << "\"type\":\"csv\","
           << "\"node_count\":" << field.node_count() << ","
           << "\"min_temperature\":" << field.min_temperature << ","
           << "\"max_temperature\":" << field.max_temperature << ","
           << "\"average_temperature\":" << field.average_temperature << ","
           << "\"bounding_box\":{"
           << "\"min\":[" << field.bounding_box.min_point.x << "," << field.bounding_box.min_point.y << "," << field.bounding_box.min_point.z << "],"
           << "\"max\":[" << field.bounding_box.max_point.x << "," << field.bounding_box.max_point.y << "," << field.bounding_box.max_point.z << "],"
           << "\"span\":[" << field.bounding_box.dimensions().x << "," << field.bounding_box.dimensions().y << "," << field.bounding_box.dimensions().z << "]"
           << "},"
           << "\"nodes\":[";

        for (size_t i = 0; i < field.nodes.size(); ++i) {
            const auto& n = field.nodes[i];
            if (i > 0) ss << ",";
            ss << "{"
               << "\"id\":" << n.node_id << ","
               << "\"x\":" << n.position.x << ","
               << "\"y\":" << n.position.y << ","
               << "\"z\":" << n.position.z << ","
               << "\"temperature_c\":" << n.temperature_c
               << "}";
        }
        ss << "]}";

        resp.success = true;
        resp.payload_json = ss.str();
    }

    return resp;
}

BridgeResponse HostBridge::handle_geometry_parse_custom(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "geometry_parse_custom_result";

    const std::string stl_text = extract_json_field(payload, "stl_text");
    const std::string csv_text = extract_json_field(payload, "csv_text");

    if (!stl_text.empty()) {
        auto res = geometry::parse_ascii_stl_string(stl_text);
        if (res.is_error()) {
            resp.success = false;
            resp.error_message = "STL Parse Error: " + res.error().message;
            return resp;
        }

        const auto& mesh = res.value();
        const size_t original_count = mesh.triangle_count();
        bool simplified = false;
        std::string simplification_label;
        size_t stride = 1;

        if (original_count > kMaxStlFacetsLimit) {
            stride = (original_count + kMaxStlFacetsLimit - 1) / kMaxStlFacetsLimit;
            simplified = true;
            simplification_label = "Simplified Visualization: Rendered every " + std::to_string(stride) +
                                   "th facet to respect 25,000 facet limit.";
        }

        std::ostringstream ss;
        ss << std::setprecision(8);
        ss << "{"
           << "\"type\":\"stl\","
           << "\"solid_name\":\"" << escape_json(mesh.solid_name) << "\","
           << "\"total_triangles\":" << original_count << ","
           << "\"degenerate_count\":" << mesh.degenerate_count << ","
           << "\"total_surface_area\":" << mesh.total_surface_area << ","
           << "\"simplified_visualization\":" << (simplified ? "true" : "false") << ","
           << "\"simplification_label\":\"" << escape_json(simplification_label) << "\","
           << "\"bounding_box\":{"
           << "\"min\":[" << mesh.bounding_box.min_point.x << "," << mesh.bounding_box.min_point.y << "," << mesh.bounding_box.min_point.z << "],"
           << "\"max\":[" << mesh.bounding_box.max_point.x << "," << mesh.bounding_box.max_point.y << "," << mesh.bounding_box.max_point.z << "],"
           << "\"span\":[" << mesh.bounding_box.dimensions().x << "," << mesh.bounding_box.dimensions().y << "," << mesh.bounding_box.dimensions().z << "]"
           << "},"
           << "\"vertices\":[";

        size_t rendered_count = 0;
        for (size_t i = 0; i < original_count; i += stride) {
            const auto& t = mesh.triangles[i];
            if (rendered_count > 0) ss << ",";
            ss << t.v1.x << "," << t.v1.y << "," << t.v1.z << ","
               << t.v2.x << "," << t.v2.y << "," << t.v2.z << ","
               << t.v3.x << "," << t.v3.y << "," << t.v3.z;
            rendered_count++;
        }
        ss << "],"
           << "\"normals\":[";
        rendered_count = 0;
        for (size_t i = 0; i < original_count; i += stride) {
            const auto& t = mesh.triangles[i];
            if (rendered_count > 0) ss << ",";
            ss << t.normal.x << "," << t.normal.y << "," << t.normal.z;
            rendered_count++;
        }
        ss << "],"
           << "\"rendered_triangles\":" << rendered_count
           << "}";

        resp.success = true;
        resp.payload_json = ss.str();
        return resp;
    } else if (!csv_text.empty()) {
        auto res = geometry::parse_temperature_csv_string(csv_text);
        if (res.is_error()) {
            resp.success = false;
            resp.error_message = "CSV Parse Error: " + res.error().message;
            return resp;
        }

        const auto& field = res.value();
        std::ostringstream ss;
        ss << std::setprecision(8);
        ss << "{"
           << "\"type\":\"csv\","
           << "\"node_count\":" << field.node_count() << ","
           << "\"min_temperature\":" << field.min_temperature << ","
           << "\"max_temperature\":" << field.max_temperature << ","
           << "\"average_temperature\":" << field.average_temperature << ","
           << "\"bounding_box\":{"
           << "\"min\":[" << field.bounding_box.min_point.x << "," << field.bounding_box.min_point.y << "," << field.bounding_box.min_point.z << "],"
           << "\"max\":[" << field.bounding_box.max_point.x << "," << field.bounding_box.max_point.y << "," << field.bounding_box.max_point.z << "],"
           << "\"span\":[" << field.bounding_box.dimensions().x << "," << field.bounding_box.dimensions().y << "," << field.bounding_box.dimensions().z << "]"
           << "},"
           << "\"nodes\":[";

        for (size_t i = 0; i < field.nodes.size(); ++i) {
            const auto& n = field.nodes[i];
            if (i > 0) ss << ",";
            ss << "{"
               << "\"id\":" << n.node_id << ","
               << "\"x\":" << n.position.x << ","
               << "\"y\":" << n.position.y << ","
               << "\"z\":" << n.position.z << ","
               << "\"temperature_c\":" << n.temperature_c
               << "}";
        }
        ss << "]}";

        resp.success = true;
        resp.payload_json = ss.str();
        return resp;
    }

    resp.success = false;
    resp.error_message = "Payload must provide 'stl_text' or 'csv_text'.";
    return resp;
}

// =============================================================================
// MODULE 2: THERMAL FEM
// =============================================================================

BridgeResponse HostBridge::handle_thermal_solve(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "thermal_solve_result";

    thermal::Thermal1DParameters params;
    params.length = extract_json_double(payload, "length", 1.0);
    params.area = extract_json_double(payload, "area", 1.0e-4);
    params.thermal_conductivity = extract_json_double(payload, "thermal_conductivity", 10.0);
    params.heat_generation = extract_json_double(payload, "heat_generation", 10000.0);
    params.left_temperature = extract_json_double(payload, "left_temperature", 100.0);
    params.right_temperature = extract_json_double(payload, "right_temperature", 20.0);
    params.element_count = static_cast<size_t>(extract_json_int(payload, "element_count", 10));

    // Input Validation
    if (!std::isfinite(params.length) || params.length <= 0.0) {
        resp.success = false;
        resp.error_message = "Invalid length: L must be strictly positive.";
        return resp;
    }
    if (!std::isfinite(params.area) || params.area <= 0.0) {
        resp.success = false;
        resp.error_message = "Invalid area: A must be strictly positive.";
        return resp;
    }
    if (!std::isfinite(params.thermal_conductivity) || params.thermal_conductivity <= 0.0) {
        resp.success = false;
        resp.error_message = "Invalid thermal conductivity: k must be strictly positive.";
        return resp;
    }
    if (!std::isfinite(params.heat_generation) ||
        !std::isfinite(params.left_temperature) ||
        !std::isfinite(params.right_temperature)) {
        resp.success = false;
        resp.error_message = "Invalid thermal boundary values: heat_generation and temperatures must be finite numbers.";
        return resp;
    }
    if (params.element_count < 1) {
        resp.success = false;
        resp.error_message = "Invalid element count: element_count must be at least 1.";
        return resp;
    }

    auto res = thermal::solve_steady_state_1d(params);
    if (res.is_error()) {
        resp.success = false;
        resp.error_message = "Thermal solver error: " + res.error().message;
        return resp;
    }

    last_fem_result_ = res.value();
    has_fem_result_ = true;

    // Dense exact analytical solution curve (100 points)
    std::ostringstream ss;
    ss << std::setprecision(10);
    ss << "{"
       << "\"element_length\":" << last_fem_result_.element_length << ","
       << "\"max_absolute_midpoint_error_C\":" << last_fem_result_.max_absolute_midpoint_error_C << ","
       << "\"expected_max_midpoint_error_C\":" << last_fem_result_.expected_max_midpoint_error_C << ","
       << "\"nodes\":[";

    for (size_t i = 0; i < last_fem_result_.nodes.size(); ++i) {
        const auto& n = last_fem_result_.nodes[i];
        if (i > 0) ss << ",";
        ss << "{"
           << "\"node_index\":" << n.node_index << ","
           << "\"x_m\":" << n.x_m << ","
           << "\"temperature_FE_C\":" << n.temperature_FE_C << ","
           << "\"temperature_exact_C\":" << n.temperature_exact_C << ","
           << "\"absolute_error_C\":" << n.absolute_error_C
           << "}";
    }
    ss << "],"
       << "\"elements\":[";

    for (size_t i = 0; i < last_fem_result_.elements.size(); ++i) {
        const auto& el = last_fem_result_.elements[i];
        if (i > 0) ss << ",";
        ss << "{"
           << "\"element_index\":" << el.element_index << ","
           << "\"x_mid_m\":" << el.x_mid_m << ","
           << "\"temperature_FE_mid_C\":" << el.temperature_FE_mid_C << ","
           << "\"temperature_exact_mid_C\":" << el.temperature_exact_mid_C << ","
           << "\"absolute_error_C\":" << el.absolute_error_C
           << "}";
    }
    ss << "],"
       << "\"exact_curve\":[";

    const size_t curve_pts = 100;
    for (size_t i = 0; i <= curve_pts; ++i) {
        double x = (params.length * static_cast<double>(i)) / static_cast<double>(curve_pts);
        double t_exact = thermal::compute_exact_temperature(x, params);
        if (i > 0) ss << ",";
        ss << "{\"x_m\":" << x << ",\"temperature_exact_C\":" << t_exact << "}";
    }
    ss << "]}";

    resp.success = true;
    resp.payload_json = ss.str();
    return resp;
}

BridgeResponse HostBridge::handle_thermal_export_csv(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "thermal_export_csv_result";

    if (!has_fem_result_) {
        resp.success = false;
        resp.error_message = "Cannot export: No FEM solution has been calculated yet.";
        return resp;
    }

    std::string filepath = extract_json_field(payload, "filepath");
    if (filepath.empty()) {
        filepath = "results/thermal/thermal_results.csv";
    }

    if (!is_safe_relative_export_path(filepath)) {
        resp.success = false;
        resp.error_message = "Rejected: Export path '" + filepath + "' is not permitted. Paths must reside within 'results/' ending in .csv without directory traversal ('..').";
        return resp;
    }

    // Ensure directory exists
    std::filesystem::path p(filepath);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    auto status = thermal::export_fem_results_to_csv(filepath, last_fem_result_);
    if (status.is_error()) {
        resp.success = false;
        resp.error_message = "Export failed: " + status.error().message;
        return resp;
    }

    resp.success = true;
    resp.payload_json = "{\"filepath\":\"" + escape_json(filepath) + "\",\"exported\":true}";
    return resp;
}

// =============================================================================
// MODULE 3: MULTICORE PERFORMANCE BENCHMARK
// =============================================================================

BridgeResponse HostBridge::handle_benchmark_get_hardware_info(const std::string& req_id, const std::string& /*payload*/) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "benchmark_hardware_info_result";

    const auto meta = performance::get_system_metadata();

    std::ostringstream ss;
    ss << "{"
       << "\"cpu_model\":\"" << escape_json(meta.cpu_model) << "\","
       << "\"physical_core_count\":" << meta.physical_core_count << ","
       << "\"logical_processor_count\":" << meta.logical_processor_count << ","
       << "\"total_ram_bytes\":" << meta.total_ram_bytes << ","
       << "\"total_ram_readable\":\"" << escape_json(meta.total_ram_readable) << "\","
       << "\"os_name\":\"" << escape_json(meta.os_name) << "\","
       << "\"compiler_info\":\"" << escape_json(meta.compiler_name_and_version) << "\","
       << "\"build_configuration\":\"" << escape_json(meta.build_configuration) << "\","
       << "\"cpp_standard\":\"" << escape_json(meta.cpp_standard)
       << "}";

    resp.success = true;
    resp.payload_json = ss.str();
    return resp;
}

BridgeResponse HostBridge::handle_benchmark_run(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "benchmark_run_result";

    performance::WorkloadConfig config;
    config.random_seed = static_cast<uint32_t>(extract_json_int(payload, "random_seed", 42));
    config.case_count = static_cast<size_t>(extract_json_int(payload, "case_count", 100));
    config.elements_per_case = static_cast<size_t>(extract_json_int(payload, "elements_per_case", 80));

    if (config.case_count == 0) config.case_count = 100;
    if (config.elements_per_case == 0) config.elements_per_case = 80;

    std::vector<size_t> worker_targets = extract_json_size_t_array(payload, "worker_targets");
    if (worker_targets.empty()) {
        worker_targets = {1, 2, 3, 4};
    }

    auto res = performance::run_benchmark_suite(config, worker_targets);
    if (res.is_error()) {
        resp.success = false;
        resp.error_message = "Benchmark execution failed: " + res.error().message;
        return resp;
    }

    last_benchmark_result_ = res.value();
    has_benchmark_result_ = true;

    std::ostringstream ss;
    ss << std::setprecision(6);
    ss << "{"
       << "\"overall_correctness_passed\":" << (last_benchmark_result_.overall_correctness_passed ? "true" : "false") << ","
       << "\"warmup_count\":" << last_benchmark_result_.warmup_count << ","
       << "\"measured_repetitions\":" << last_benchmark_result_.measured_repetitions << ","
       << "\"case_count\":" << config.case_count << ","
       << "\"elements_per_case\":" << config.elements_per_case << ","
       << "\"summaries\":[";

    for (size_t i = 0; i < last_benchmark_result_.summaries.size(); ++i) {
        const auto& s = last_benchmark_result_.summaries[i];
        if (i > 0) ss << ",";
        ss << "{"
           << "\"execution_mode\":\"" << escape_json(s.execution_mode) << "\","
           << "\"requested_workers\":" << s.requested_workers << ","
           << "\"actual_workers\":" << s.actual_workers << ","
           << "\"median_time_ms\":" << s.median_time_ms << ","
           << "\"speedup\":" << s.speedup << ","
           << "\"efficiency_percent\":" << s.efficiency_percent << ","
           << "\"all_repetitions_correct\":" << (s.all_repetitions_correct ? "true" : "false") << ","
           << "\"note\":\"" << escape_json(s.note) << "\""
           << "}";
    }
    ss << "],"
       << "\"raw_records\":[";

    for (size_t i = 0; i < last_benchmark_result_.raw_records.size(); ++i) {
        const auto& r = last_benchmark_result_.raw_records[i];
        if (i > 0) ss << ",";
        ss << "{"
           << "\"execution_mode\":\"" << escape_json(r.execution_mode) << "\","
           << "\"repetition_index\":" << r.repetition_index << ","
           << "\"requested_workers\":" << r.requested_workers << ","
           << "\"elapsed_time_ms\":" << r.elapsed_time_ms << ","
           << "\"max_temperature_difference_C\":" << r.max_temperature_difference_C << ","
           << "\"correctness_passed\":" << (r.correctness_passed ? "true" : "false")
           << "}";
    }
    ss << "]}";

    resp.success = true;
    resp.payload_json = ss.str();
    return resp;
}

BridgeResponse HostBridge::handle_benchmark_export_csv(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "benchmark_export_csv_result";

    if (!has_benchmark_result_) {
        resp.success = false;
        resp.error_message = "Cannot export: No benchmark suite has been run yet.";
        return resp;
    }

    std::string raw_path = extract_json_field(payload, "raw_path");
    if (raw_path.empty()) raw_path = "results/benchmark/benchmark_raw.csv";
    std::string summary_path = extract_json_field(payload, "summary_path");
    if (summary_path.empty()) summary_path = "results/benchmark/benchmark_summary.csv";

    if (!is_safe_relative_export_path(raw_path) || !is_safe_relative_export_path(summary_path)) {
        resp.success = false;
        resp.error_message = "Rejected: Export paths must reside within 'results/' ending in .csv without directory traversal ('..').";
        return resp;
    }

    // Ensure directories exist
    for (const auto& p_str : {raw_path, summary_path}) {
        std::filesystem::path p(p_str);
        if (p.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
        }
    }

    auto status1 = performance::export_raw_benchmark_csv(raw_path, last_benchmark_result_);
    auto status2 = performance::export_summary_benchmark_csv(summary_path, last_benchmark_result_);

    if (status1.is_error() || status2.is_error()) {
        resp.success = false;
        resp.error_message = "Export failed: " + (status1.is_error() ? status1.error().message : status2.error().message);
        return resp;
    }

    resp.success = true;
    resp.payload_json = "{\"raw_path\":\"" + escape_json(raw_path) + "\",\"summary_path\":\"" + escape_json(summary_path) + "\",\"exported\":true}";
    return resp;
}

// =============================================================================
// MODULE 4: HARDWARE INTEGRATION
// =============================================================================

BridgeResponse HostBridge::handle_hardware_discover(const std::string& req_id, const std::string& /*payload*/) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "hardware_discover_result";

    if (!hardware_manager_) {
        hardware_manager_ = hardware::create_default_hardware_manager();
    }

    const auto devices = hardware_manager_->discover_devices();

    std::ostringstream ss;
    ss << "{\"devices\":[";
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];
        if (i > 0) ss << ",";
        auto state_res = hardware_manager_->get_device_state(d.device_id);
        std::string st_str = state_res.is_ok() ? hardware::connection_state_to_string(state_res.value()) : "Unknown";

        ss << "{"
           << "\"device_id\":\"" << escape_json(d.device_id) << "\","
           << "\"model_name\":\"" << escape_json(d.model_name) << "\","
           << "\"manufacturer\":\"" << escape_json(d.manufacturer) << "\","
           << "\"serial_number\":\"" << escape_json(d.serial_number) << "\","
           << "\"is_simulated\":" << (d.is_simulated ? "true" : "false") << ","
           << "\"firmware_version\":\"" << escape_json(d.firmware_version) << "\","
           << "\"physical_interface\":\"" << escape_json(d.physical_interface) << "\","
           << "\"device_type\":\"" << hardware::device_type_to_string(d.type) << "\","
           << "\"connection_state\":\"" << st_str << "\""
           << "}";
    }
    ss << "]}";

    resp.success = true;
    resp.payload_json = ss.str();
    return resp;
}

BridgeResponse HostBridge::handle_hardware_connect(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "hardware_connect_result";

    const std::string device_id = extract_json_field(payload, "device_id");
    if (device_id.empty()) {
        resp.success = false;
        resp.error_message = "Missing required 'device_id'.";
        return resp;
    }

    hardware::ConnectionConfig config;
    config.baud_rate = static_cast<uint32_t>(extract_json_int(payload, "baud_rate", 115200));
    config.timeout_ms = static_cast<uint32_t>(extract_json_int(payload, "timeout_ms", 1000));
    config.simulate_timeout = extract_json_bool(payload, "simulate_timeout", false);
    config.simulate_connection_failure = extract_json_bool(payload, "simulate_connection_failure", false);

    if (!hardware_manager_) {
        hardware_manager_ = hardware::create_default_hardware_manager();
    }

    auto status = hardware_manager_->connect_device(device_id, config);
    auto state_res = hardware_manager_->get_device_state(device_id);
    std::string state_str = state_res.is_ok() ? hardware::connection_state_to_string(state_res.value()) : "Unknown";

    if (status.is_error()) {
        resp.success = false;
        resp.error_message = status.error().message;
        resp.payload_json = "{\"device_id\":\"" + escape_json(device_id) + "\",\"connection_state\":\"" + state_str + "\"}";
        return resp;
    }

    resp.success = true;
    resp.payload_json = "{\"device_id\":\"" + escape_json(device_id) + "\",\"connection_state\":\"" + state_str + "\"}";
    return resp;
}

BridgeResponse HostBridge::handle_hardware_disconnect(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "hardware_disconnect_result";

    const std::string device_id = extract_json_field(payload, "device_id");
    if (device_id.empty()) {
        resp.success = false;
        resp.error_message = "Missing required 'device_id'.";
        return resp;
    }

    if (!hardware_manager_) {
        hardware_manager_ = hardware::create_default_hardware_manager();
    }

    auto status = hardware_manager_->disconnect_device(device_id);
    auto state_res = hardware_manager_->get_device_state(device_id);
    std::string state_str = state_res.is_ok() ? hardware::connection_state_to_string(state_res.value()) : "Disconnected";

    if (status.is_error()) {
        resp.success = false;
        resp.error_message = status.error().message;
        return resp;
    }

    resp.success = true;
    resp.payload_json = "{\"device_id\":\"" + escape_json(device_id) + "\",\"connection_state\":\"" + state_str + "\"}";
    return resp;
}

BridgeResponse HostBridge::handle_hardware_read_telemetry(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "hardware_telemetry_result";

    const std::string device_id = extract_json_field(payload, "device_id");
    if (device_id.empty()) {
        resp.success = false;
        resp.error_message = "Missing required 'device_id'.";
        return resp;
    }

    if (!hardware_manager_) {
        hardware_manager_ = hardware::create_default_hardware_manager();
    }

    auto res = hardware_manager_->read_device_telemetry(device_id);
    if (res.is_error()) {
        resp.success = false;
        resp.error_message = res.error().message;
        return resp;
    }

    const auto& msg = res.value();
    std::ostringstream ss;
    ss << std::setprecision(4);
    ss << "{"
       << "\"device_id\":\"" << escape_json(msg.device_id) << "\","
       << "\"timestamp_ms\":" << msg.timestamp_ms << ","
       << "\"sequence_number\":" << msg.sequence_number << ","
       << "\"core_voltage_v\":" << msg.core_voltage_v << ","
       << "\"auxiliary_voltage_v\":" << msg.auxiliary_voltage_v << ","
       << "\"board_temperature_c\":" << msg.board_temperature_c << ","
       << "\"power_consumption_w\":" << msg.power_consumption_w << ","
       << "\"status_flag\":\"" << escape_json(msg.status_flag) << "\","
       << "\"is_simulated\":true"
       << "}";

    resp.success = true;
    resp.payload_json = ss.str();
    return resp;
}

BridgeResponse HostBridge::handle_hardware_send_command(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "hardware_command_result";

    const std::string device_id = extract_json_field(payload, "device_id");
    const std::string command = extract_json_field(payload, "command");
    const std::string cmd_payload = extract_json_field(payload, "cmd_payload");

    if (device_id.empty() || command.empty()) {
        resp.success = false;
        resp.error_message = "Missing 'device_id' or 'command'.";
        return resp;
    }

    if (!hardware_manager_) {
        hardware_manager_ = hardware::create_default_hardware_manager();
    }

    auto status = hardware_manager_->execute_device_command(device_id, command, cmd_payload);
    if (status.is_error()) {
        resp.success = false;
        resp.error_message = status.error().message;
        return resp;
    }

    resp.success = true;
    resp.payload_json = "{\"device_id\":\"" + escape_json(device_id) + "\",\"command\":\"" + escape_json(command) + "\",\"executed\":true}";
    return resp;
}

BridgeResponse HostBridge::handle_hardware_reset_simulation_flags(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "hardware_reset_flags_result";

    const std::string device_id = extract_json_field(payload, "device_id");
    if (device_id.empty()) {
        resp.success = false;
        resp.error_message = "Missing required 'device_id'.";
        return resp;
    }

    if (!hardware_manager_) {
        hardware_manager_ = hardware::create_default_hardware_manager();
    }

    auto adapter = hardware_manager_->get_adapter(device_id);
    if (!adapter) {
        resp.success = false;
        resp.error_message = "Device adapter not found: " + device_id;
        return resp;
    }

    adapter->reset_simulation_flags();
    hardware_manager_->disconnect_device(device_id);

    resp.success = true;
    resp.payload_json = "{\"device_id\":\"" + escape_json(device_id) + "\",\"flags_reset\":true,\"connection_state\":\"Disconnected\"}";
    return resp;
}

// =============================================================================
// MODULE 5: BACKPROPAGATION & NUMERICAL VERIFICATION
// =============================================================================

BridgeResponse HostBridge::handle_backprop_verify_gradients(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "backprop_verify_gradients_result";

    double x = extract_json_double(payload, "x", 1.0);
    double y = extract_json_double(payload, "y", 2.0);

    backprop::NetworkParameters params;
    params.w = extract_json_double(payload, "w", 0.5);
    params.b = extract_json_double(payload, "b", 0.0);
    params.v = extract_json_double(payload, "v", 1.0);
    params.c = extract_json_double(payload, "c", 0.0);

    double eps = extract_json_double(payload, "epsilon", 1.0e-6);
    double tol = extract_json_double(payload, "tolerance", 1.0e-5);

    if (!std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(params.w) || !std::isfinite(params.b) ||
        !std::isfinite(params.v) || !std::isfinite(params.c)) {
        resp.success = false;
        resp.error_message = "Parameters and sample inputs must be finite numbers.";
        return resp;
    }

    auto verif = backprop::verify_gradients(x, y, params, eps, tol);

    std::ostringstream ss;
    ss << std::setprecision(12);
    ss << "{"
       << "\"passed\":" << (verif.passed ? "true" : "false") << ","
       << "\"max_absolute_error\":" << verif.max_absolute_error << ","
       << "\"tolerance\":" << tol << ","
       << "\"epsilon\":" << eps << ","
       << "\"analytical\":{"
       << "\"dw\":" << verif.analytical.dw << ","
       << "\"db\":" << verif.analytical.db << ","
       << "\"dv\":" << verif.analytical.dv << ","
       << "\"dc\":" << verif.analytical.dc
       << "},"
       << "\"numerical\":{"
       << "\"dw\":" << verif.numerical.dw << ","
       << "\"db\":" << verif.numerical.db << ","
       << "\"dv\":" << verif.numerical.dv << ","
       << "\"dc\":" << verif.numerical.dc
       << "},"
       << "\"absolute_errors\":{"
       << "\"dw\":" << verif.absolute_errors.dw << ","
       << "\"db\":" << verif.absolute_errors.db << ","
       << "\"dv\":" << verif.absolute_errors.dv << ","
       << "\"dc\":" << verif.absolute_errors.dc
       << "}"
       << "}";

    resp.success = true;
    resp.payload_json = ss.str();
    return resp;
}

BridgeResponse HostBridge::handle_backprop_train(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "backprop_train_result";

    double x = extract_json_double(payload, "x", 1.0);
    double y = extract_json_double(payload, "y", 2.0);

    backprop::NetworkParameters params;
    params.w = extract_json_double(payload, "w", 0.5);
    params.b = extract_json_double(payload, "b", 0.0);
    params.v = extract_json_double(payload, "v", 1.0);
    params.c = extract_json_double(payload, "c", 0.0);

    double eta = extract_json_double(payload, "learning_rate", 0.1);
    size_t steps = static_cast<size_t>(extract_json_int(payload, "steps", 100));

    if (!std::isfinite(eta) || eta <= 0.0) {
        resp.success = false;
        resp.error_message = "Learning rate (eta) must be strictly positive.";
        return resp;
    }
    if (steps == 0 || steps > 10000) {
        resp.success = false;
        resp.error_message = "Steps must be between 1 and 10000.";
        return resp;
    }

    auto history = backprop::train_network(x, y, params, eta, steps);
    last_training_history_ = history;
    has_training_history_ = true;

    std::ostringstream ss;
    ss << std::setprecision(10);
    ss << "{"
       << "\"step_count\":" << history.size() << ","
       << "\"initial_loss\":" << (history.empty() ? 0.0 : history.front().loss) << ","
       << "\"final_loss\":" << (history.empty() ? 0.0 : history.back().loss) << ","
       << "\"records\":[";

    for (size_t i = 0; i < history.size(); ++i) {
        const auto& r = history[i];
        if (i > 0) ss << ",";
        ss << "{"
           << "\"step\":" << r.step << ","
           << "\"loss\":" << r.loss << ","
           << "\"y_pred\":" << r.y_pred << ","
           << "\"w\":" << r.parameters.w << ","
           << "\"b\":" << r.parameters.b << ","
           << "\"v\":" << r.parameters.v << ","
           << "\"c\":" << r.parameters.c << ","
           << "\"dw\":" << r.gradients.dw << ","
           << "\"db\":" << r.gradients.db << ","
           << "\"dv\":" << r.gradients.dv << ","
           << "\"dc\":" << r.gradients.dc
           << "}";
    }
    ss << "]}";

    resp.success = true;
    resp.payload_json = ss.str();
    return resp;
}

BridgeResponse HostBridge::handle_backprop_step(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "backprop_step_result";

    double x = extract_json_double(payload, "x", 1.0);
    double y = extract_json_double(payload, "y", 2.0);

    backprop::NetworkParameters params;
    params.w = extract_json_double(payload, "w", 0.5);
    params.b = extract_json_double(payload, "b", 0.0);
    params.v = extract_json_double(payload, "v", 1.0);
    params.c = extract_json_double(payload, "c", 0.0);

    double eta = extract_json_double(payload, "learning_rate", 0.1);

    auto fwd = backprop::forward_pass(x, y, params);
    auto grads = backprop::compute_analytical_gradients(x, y, params);
    auto next_params = backprop::update_parameters_simultaneous(params, grads, eta);

    std::ostringstream ss;
    ss << std::setprecision(10);
    ss << "{"
       << "\"z\":" << fwd.z << ","
       << "\"h\":" << fwd.h << ","
       << "\"y_pred\":" << fwd.y_pred << ","
       << "\"loss\":" << fwd.loss << ","
       << "\"gradients\":{\"dw\":" << grads.dw << ",\"db\":" << grads.db << ",\"dv\":" << grads.dv << ",\"dc\":" << grads.dc << "},"
       << "\"updated_parameters\":{\"w\":" << next_params.w << ",\"b\":" << next_params.b << ",\"v\":" << next_params.v << ",\"c\":" << next_params.c << "}"
       << "}";

    resp.success = true;
    resp.payload_json = ss.str();
    return resp;
}

BridgeResponse HostBridge::handle_backprop_export_csv(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "backprop_export_csv_result";

    if (!has_training_history_ || last_training_history_.empty()) {
        resp.success = false;
        resp.error_message = "Cannot export: No training trajectory has been run yet.";
        return resp;
    }

    std::string filepath = extract_json_field(payload, "filepath");
    if (filepath.empty()) filepath = "results/backprop/loss_history.csv";

    if (!is_safe_relative_export_path(filepath)) {
        resp.success = false;
        resp.error_message = "Rejected: Export path '" + filepath + "' is not permitted. Paths must reside within 'results/' ending in .csv without directory traversal ('..').";
        return resp;
    }

    std::filesystem::path p(filepath);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    auto status = backprop::export_loss_history_csv(filepath, last_training_history_);
    if (status.is_error()) {
        resp.success = false;
        resp.error_message = "Export failed: " + status.error().message;
        return resp;
    }

    resp.success = true;
    resp.payload_json = "{\"filepath\":\"" + escape_json(filepath) + "\",\"exported\":true}";
    return resp;
}

BridgeResponse HostBridge::handle_dialog_open_file(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "dialog_open_file_result";

    if (extract_json_bool(payload, "simulate_cancel", false)) {
        resp.success = true;
        resp.payload_json = "{\"cancelled\":true}";
        return resp;
    }

    std::string file_path = extract_json_field(payload, "file_path");
    if (file_path.empty()) {
        resp.success = true;
        resp.payload_json = "{\"cancelled\":true}";
        return resp;
    }

    // Disallow directory traversal
    if (file_path.find("..") != std::string::npos) {
        resp.success = false;
        resp.error_message = "Rejected: File path contains forbidden traversal sequence ('..').";
        return resp;
    }

    std::string ext = extract_json_field(payload, "filter_ext");
    if (!ext.empty()) {
        if (file_path.size() < ext.size() || file_path.substr(file_path.size() - ext.size()) != ext) {
            resp.success = false;
            resp.error_message = "Rejected: Selected file does not have expected extension: " + ext;
            return resp;
        }
    }

    resp.success = true;
    resp.payload_json = "{\"cancelled\":false,\"file_path\":\"" + escape_json(file_path) + "\"}";
    return resp;
}

BridgeResponse HostBridge::handle_dialog_save_file(const std::string& req_id, const std::string& payload) {
    BridgeResponse resp;
    resp.request_id = req_id;
    resp.action = "dialog_save_file_result";

    if (extract_json_bool(payload, "simulate_cancel", false)) {
        resp.success = true;
        resp.payload_json = "{\"cancelled\":true}";
        return resp;
    }

    std::string file_path = extract_json_field(payload, "file_path");
    if (file_path.empty()) {
        std::string def_name = extract_json_field(payload, "default_filename");
        if (def_name.empty()) def_name = "export.csv";
        file_path = "results/" + def_name;
    }

    if (!is_safe_relative_export_path(file_path)) {
        resp.success = false;
        resp.error_message = "Rejected: Save path must be within 'results/' ending in .csv without directory traversal ('..').";
        return resp;
    }

    resp.success = true;
    resp.payload_json = "{\"cancelled\":false,\"file_path\":\"" + escape_json(file_path) + "\"}";
    return resp;
}

} // namespace workbench::ui
