#include "modules/hardware/stm32_adapter.h"

#include <chrono>
#include <cmath>

namespace workbench::hardware {

MockStm32DiscoveryAdapter::MockStm32DiscoveryAdapter() {
    m_descriptor.device_id = "stm32f407g-disc1-01";
    m_descriptor.model_name = "STM32F407G-DISC1 (SIMULATED)";
    m_descriptor.manufacturer = "STMicroelectronics";
    m_descriptor.serial_number = "SIM-STM32F407-0042";
    m_descriptor.is_simulated = true;
    m_descriptor.firmware_version = "ST-LINK/V2-A v2.J37.M26 (SIMULATED)";
    m_descriptor.physical_interface = "USB Mini-B (ST-LINK/V2-A SWD Interface)";
    m_descriptor.type = DeviceType::Microcontroller;

    m_capabilities.supports_swd = true;
    m_capabilities.supports_jtag = false; // Verified fact: onboard ST-LINK uses SWD; no 20-pin ARM JTAG header
    m_capabilities.supports_uart_telemetry = true;
    m_capabilities.supports_fpga_bitstream_flashing = false;
    m_capabilities.supports_mcu_firmware_flashing = true;
    m_capabilities.supports_voltage_monitoring = true;
    m_capabilities.nominal_logic_voltage_v = 3.3;
    m_capabilities.debug_protocol = "SWD (Serial Wire Debug)";
    m_capabilities.connector_type = "USB Mini-B to Onboard ST-LINK/V2-A (SWCLK/SWDIO/NRST/SWO)";
}

const DeviceDescriptor& MockStm32DiscoveryAdapter::descriptor() const {
    return m_descriptor;
}

const DeviceCapabilities& MockStm32DiscoveryAdapter::capabilities() const {
    return m_capabilities;
}

ConnectionState MockStm32DiscoveryAdapter::state() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

core::Status MockStm32DiscoveryAdapter::connect(const ConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (config.baud_rate == 0) {
        return core::Status::failure(
            core::StatusCode::InvalidInput,
            "Cannot connect STM32F407G-DISC1: baud rate must be greater than 0."
        );
    }

    if (config.timeout_ms == 0) {
        return core::Status::failure(
            core::StatusCode::InvalidInput,
            "Cannot connect STM32F407G-DISC1: timeout must be greater than 0 ms."
        );
    }

    m_config = config;
    if (config.simulate_timeout) {
        m_force_timeout = true;
    }
    if (config.simulate_connection_failure) {
        m_force_failure = true;
    }

    // Simulated Timeout Injection
    if (m_force_timeout) {
        m_state = ConnectionState::Error;
        return core::Status::failure(
            core::StatusCode::Timeout,
            "Simulated connection timeout: STM32F407G-DISC1 failed to respond within " +
            std::to_string(config.timeout_ms) + " ms."
        );
    }

    // Simulated Connection Failure Injection
    if (m_force_failure) {
        m_state = ConnectionState::Error;
        return core::Status::failure(
            core::StatusCode::ConnectionFailed,
            "Simulated connection failure: ST-LINK/V2-A USB enumeration failed (SIMULATED)."
        );
    }

    m_state = ConnectionState::Connected;
    return core::Status::ok();
}

core::Status MockStm32DiscoveryAdapter::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = ConnectionState::Disconnected;
    return core::Status::ok();
}

core::Result<TelemetryMessage> MockStm32DiscoveryAdapter::read_telemetry() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != ConnectionState::Connected) {
        return core::Result<TelemetryMessage>::failure(
            core::StatusCode::DeviceDisconnected,
            "Cannot read telemetry: STM32F407G-DISC1 is not connected (current state: " +
            std::string(connection_state_to_string(m_state)) + ")."
        );
    }

    m_sequence_counter++;

    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    TelemetryMessage msg;
    msg.device_id = m_descriptor.device_id;
    msg.timestamp_ms = static_cast<uint64_t>(ms);
    msg.sequence_number = m_sequence_counter;
    msg.is_simulated = true;

    // Physically plausible STM32F407 values
    // VDD = 3.3V nominal, VCORE = 1.2V nominal, MCU temp ~ 38.5 C
    const double seq_offset = static_cast<double>(m_sequence_counter % 20);
    msg.core_voltage_v = 1.20 + 0.005 * std::sin(seq_offset * 0.314);
    msg.auxiliary_voltage_v = 3.30 + 0.01 * std::cos(seq_offset * 0.314);
    msg.board_temperature_c = 38.5 + 0.8 * std::sin(seq_offset * 0.5);
    msg.power_consumption_w = 0.33 + 0.02 * std::cos(seq_offset * 0.2);
    msg.status_flag = "NORMAL_RUN_MODE (SIMULATED)";

    return core::Result<TelemetryMessage>::success(msg);
}

core::Status MockStm32DiscoveryAdapter::execute_command(const std::string& command, const std::string& /*payload*/) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != ConnectionState::Connected) {
        return core::Status::failure(
            core::StatusCode::DeviceDisconnected,
            "Cannot execute command '" + command + "': STM32F407G-DISC1 is not connected."
        );
    }

    if (command == "ping") {
        return core::Status::ok();
    }
    if (command == "flash_firmware") {
        return core::Status::ok();
    }
    if (command == "read_mcu_registers") {
        return core::Status::ok();
    }
    if (command == "reset_target") {
        return core::Status::ok();
    }

    // Explicit unsupported capability rejections
    if (command == "flash_fpga_bitstream") {
        return core::Status::failure(
            core::StatusCode::UnsupportedOperation,
            "Command 'flash_fpga_bitstream' unsupported: STM32F407G-DISC1 is an ARM Cortex-M4 microcontroller without an FPGA fabric."
        );
    }
    if (command == "jtag_boundary_scan") {
        return core::Status::failure(
            core::StatusCode::UnsupportedOperation,
            "Command 'jtag_boundary_scan' unsupported: STM32F407G-DISC1 uses ST-LINK/V2-A SWD and does not expose a standard JTAG scan chain."
        );
    }

    return core::Status::failure(
        core::StatusCode::UnsupportedOperation,
        "Command '" + command + "' is not supported by STM32F407G-DISC1 adapter."
    );
}

void MockStm32DiscoveryAdapter::reset_simulation_flags() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_force_timeout = false;
    m_force_failure = false;
}

} // namespace workbench::hardware
