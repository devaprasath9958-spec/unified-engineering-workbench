#ifndef WORKBENCH_UI_BRIDGE_HOST_BRIDGE_H
#define WORKBENCH_UI_BRIDGE_HOST_BRIDGE_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstddef>
#include "core/result.h"
#include "modules/thermal/fem_1d_solver.h"
#include "modules/performance/benchmark_runner.h"
#include "modules/hardware/hardware_manager.h"
#include "modules/backprop/network_types.h"

namespace workbench::ui {

/**
 * @brief Response structure returned to the frontend.
 */
struct BridgeResponse {
    std::string request_id;
    bool success{false};
    std::string action;
    std::string payload_json;
    std::string error_message;

    std::string to_json() const;
};

/**
 * @brief Status information dynamically queried from the C++ host.
 */
struct HostSystemStatus {
    std::string app_name{"Unified Engineering Workbench"};
    std::string app_version{"1.0.0"};
    std::string host_platform{"Windows 11 x64"};
    std::string cpp_standard{"C++17"};
    std::string compiler_info;
    std::string webview_runtime_version;
    std::vector<std::string> loaded_modules;
    bool backend_ready{true};
    int64_t timestamp_ms{0};
};

/**
 * @brief Secure Host Bridge for handling IPC messages between WebView2 and C++17 modules.
 * 
 * Security properties & limits:
 * - Strict incoming message size limit (64 KB).
 * - Safe outgoing response size limit (4 MB).
 * - STL visualization limit: 25,000 facets (with clear simplification labeling).
 * - Exact origin validation (local file or trusted scheme).
 * - Strict action and payload allowlists.
 * - Rejection of arbitrary shell commands or untrusted file paths.
 */
class HostBridge {
public:
    static constexpr size_t kMaxMessageSizeBytes = 65536;        // 64 KB incoming limit
    static constexpr size_t kMaxOutgoingMessageSizeBytes = 4194304; // 4 MB outgoing limit
    static constexpr size_t kMaxStlFacetsLimit = 25000;         // 25,000 facet limit

    HostBridge();
    ~HostBridge() = default;

    // Non-copyable to protect internal module state
    HostBridge(const HostBridge&) = delete;
    HostBridge& operator=(const HostBridge&) = delete;
    HostBridge(HostBridge&&) = default;
    HostBridge& operator=(HostBridge&&) = default;

    /**
     * @brief Set runtime info from the host.
     */
    void set_webview_runtime_version(const std::string& version);
    void set_compiler_info(const std::string& info);

    /**
     * @brief Validate and process an incoming raw JSON message from the frontend.
     * @param raw_message Raw string message received from WebView2.
     * @param origin URI origin of the sender.
     * @return Formatted JSON response string to send back.
     */
    std::string process_message(const std::string& raw_message, const std::string& origin);

    /**
     * @brief Check whether a message origin is permitted.
     */
    static bool is_origin_allowed(const std::string& origin);

    /**
     * @brief Verify that an export file path is safe (relative path within results/, no directory traversal, .csv extension).
     */
    static bool is_safe_relative_export_path(const std::string& path);

    /**
     * @brief Get current dynamic system status.
     */
    HostSystemStatus get_status() const;

    /**
     * @brief Direct access to hardware manager (for testing or lifecycle coordination).
     */
    hardware::HardwareManager* get_hardware_manager() { return hardware_manager_.get(); }

private:
    std::string webview_runtime_version_{"Unknown"};
    std::string compiler_info_{"MSVC x64"};
    std::vector<std::string> loaded_modules_;

    // Module instances & caches for stateful operations (exports, hardware)
    std::unique_ptr<hardware::HardwareManager> hardware_manager_;
    thermal::Fem1DResult last_fem_result_;
    bool has_fem_result_{false};
    performance::BenchmarkSuiteResult last_benchmark_result_;
    bool has_benchmark_result_{false};
    std::vector<backprop::TrainingStepRecord> last_training_history_;
    bool has_training_history_{false};

    // Action Handlers
    BridgeResponse handle_ping(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_handshake(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_get_status(const std::string& req_id, const std::string& payload);

    // Module 1: Geometry Handlers
    BridgeResponse handle_geometry_load_sample(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_geometry_parse_custom(const std::string& req_id, const std::string& payload);

    // Module 2: Thermal FEM Handlers
    BridgeResponse handle_thermal_solve(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_thermal_export_csv(const std::string& req_id, const std::string& payload);

    // Module 3: Multicore Performance Handlers
    BridgeResponse handle_benchmark_get_hardware_info(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_benchmark_run(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_benchmark_export_csv(const std::string& req_id, const std::string& payload);

    // Module 4: Hardware Integration Handlers
    BridgeResponse handle_hardware_discover(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_hardware_connect(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_hardware_disconnect(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_hardware_read_telemetry(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_hardware_send_command(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_hardware_reset_simulation_flags(const std::string& req_id, const std::string& payload);

    // Module 5: Backpropagation Handlers
    BridgeResponse handle_backprop_verify_gradients(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_backprop_train(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_backprop_step(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_backprop_export_csv(const std::string& req_id, const std::string& payload);

    // Native File Dialog Handlers
    BridgeResponse handle_dialog_open_file(const std::string& req_id, const std::string& payload);
    BridgeResponse handle_dialog_save_file(const std::string& req_id, const std::string& payload);
};

} // namespace workbench::ui

#endif // WORKBENCH_UI_BRIDGE_HOST_BRIDGE_H
