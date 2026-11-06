#pragma once

#include "core/result.h"
#include "modules/hardware/hardware_types.h"

#include <string>

namespace workbench::hardware {

/// @brief Abstract base interface for hardware device adapters (physical or simulated).
class IHardwareAdapter {
public:
    virtual ~IHardwareAdapter() = default;

    /// @brief Returns the static identification and metadata descriptor of the device.
    virtual const DeviceDescriptor& descriptor() const = 0;

    /// @brief Returns the functional capabilities supported by this adapter.
    virtual const DeviceCapabilities& capabilities() const = 0;

    /// @brief Returns the current connection state of the adapter.
    virtual ConnectionState state() const = 0;

    /// @brief Establishes a connection to the target device.
    /// @param config Connection settings including timeout and simulation fault injection flags.
    /// @return Status indicating success or specific failure reason (e.g., Timeout, ConnectionFailed).
    virtual core::Status connect(const ConnectionConfig& config = {}) = 0;

    /// @brief Terminates the active connection to the target device.
    virtual core::Status disconnect() = 0;

    /// @brief Queries the latest telemetry packet from the target device.
    /// @return Result containing TelemetryMessage if connected, or error status if disconnected.
    virtual core::Result<TelemetryMessage> read_telemetry() = 0;

    /// @brief Dispatches a device-specific command to the target.
    /// @param command Command identifier (e.g., "flash_firmware", "xadc_read").
    /// @param payload Optional payload string or configuration argument.
    /// @return Status::ok() on success, or StatusCode::UnsupportedOperation if unsupported.
    virtual core::Status execute_command(const std::string& command, const std::string& payload = "") = 0;

    /// @brief Clears simulated failure or timeout flags, enabling clean recovery on subsequent operations.
    virtual void reset_simulation_flags() = 0;
};

} // namespace workbench::hardware
