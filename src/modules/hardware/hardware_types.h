#pragma once

#include <string>
#include <cstdint>

namespace workbench::hardware {

/// @brief Classification of physical or simulated hardware target device.
enum class DeviceType {
    Microcontroller,
    FpgaBoard,
    JtagAdapter
};

/// @brief Returns a string representation of the device type.
inline const char* device_type_to_string(DeviceType type) {
    switch (type) {
        case DeviceType::Microcontroller: return "Microcontroller";
        case DeviceType::FpgaBoard:       return "FpgaBoard";
        case DeviceType::JtagAdapter:     return "JtagAdapter";
        default:                          return "Unknown";
    }
}

/// @brief State of communication channel between host software and device adapter.
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

/// @brief Returns a human-readable name of the connection state.
inline const char* connection_state_to_string(ConnectionState state) {
    switch (state) {
        case ConnectionState::Disconnected:  return "Disconnected";
        case ConnectionState::Connecting:    return "Connecting";
        case ConnectionState::Connected:     return "Connected";
        case ConnectionState::Disconnecting: return "Disconnecting";
        case ConnectionState::Error:         return "Error";
        default:                             return "Unknown";
    }
}

/// @brief Identification and hardware provenance descriptor.
struct DeviceDescriptor {
    std::string device_id;
    std::string model_name;
    std::string manufacturer;
    std::string serial_number;
    bool is_simulated = true;  ///< Explicit simulation flag
    std::string firmware_version;
    std::string physical_interface;
    DeviceType type = DeviceType::Microcontroller;
};

/// @brief Functional capabilities supported by the device adapter.
struct DeviceCapabilities {
    bool supports_swd = false;
    bool supports_jtag = false;
    bool supports_uart_telemetry = false;
    bool supports_fpga_bitstream_flashing = false;
    bool supports_mcu_firmware_flashing = false;
    bool supports_voltage_monitoring = false;
    double nominal_logic_voltage_v = 3.3;
    std::string debug_protocol = "None";
    std::string connector_type = "None";
};

/// @brief Parameters and simulation fault-injection flags for establishing a connection.
struct ConnectionConfig {
    uint32_t baud_rate = 115200;
    uint32_t timeout_ms = 1000;
    bool simulate_timeout = false;            ///< Injects a simulated timeout during connect
    bool simulate_connection_failure = false;  ///< Injects a simulated link failure during connect
};

/// @brief Telemetry packet containing timestamped status and sensory readouts.
struct TelemetryMessage {
    std::string device_id;
    uint64_t timestamp_ms = 0;
    uint32_t sequence_number = 0;
    double core_voltage_v = 0.0;
    double auxiliary_voltage_v = 0.0;
    double board_temperature_c = 0.0;
    double power_consumption_w = 0.0;
    std::string status_flag = "OK (SIMULATED)";
    bool is_simulated = true;  ///< Explicit simulation flag
};

} // namespace workbench::hardware
