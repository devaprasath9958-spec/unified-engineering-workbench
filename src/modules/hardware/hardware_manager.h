#pragma once

#include "core/result.h"
#include "modules/hardware/hardware_adapter.h"

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>

namespace workbench::hardware {

/// @brief Thread-safe central manager and discovery registry for hardware device adapters.
class HardwareManager {
public:
    HardwareManager() = default;
    ~HardwareManager();

    // Prevent copy/move to enforce single instance ownership
    HardwareManager(const HardwareManager&) = delete;
    HardwareManager& operator=(const HardwareManager&) = delete;

    /// @brief Registers a hardware adapter instance into the registry.
    void register_adapter(std::shared_ptr<IHardwareAdapter> adapter);

    /// @brief Discovers and returns descriptors of all registered target devices.
    std::vector<DeviceDescriptor> discover_devices() const;

    /// @brief Retrieves a shared pointer to a registered adapter by device ID.
    std::shared_ptr<IHardwareAdapter> get_adapter(const std::string& device_id) const;

    /// @brief Connects to a target device without holding the global registry lock during I/O.
    core::Status connect_device(const std::string& device_id, const ConnectionConfig& config = {});

    /// @brief Disconnects a target device.
    core::Status disconnect_device(const std::string& device_id);

    /// @brief Reads telemetry from a specific device.
    core::Result<TelemetryMessage> read_device_telemetry(const std::string& device_id);

    /// @brief Dispatches a command to a specific device.
    core::Status execute_device_command(const std::string& device_id, const std::string& command, const std::string& payload = "");

    /// @brief Queries current connection state of a device.
    core::Result<ConnectionState> get_device_state(const std::string& device_id) const;

    /// @brief Returns a list of device IDs that are currently in Connected state.
    std::vector<std::string> connected_device_ids() const;

    /// @brief Safely terminates connections to all registered devices.
    void disconnect_all();

private:
    mutable std::mutex m_registry_mutex;
    std::unordered_map<std::string, std::shared_ptr<IHardwareAdapter>> m_adapters;
};

/// @brief Factory function creating a pre-populated HardwareManager with default mock adapters.
std::unique_ptr<HardwareManager> create_default_hardware_manager();

} // namespace workbench::hardware
