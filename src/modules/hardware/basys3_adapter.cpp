#include "modules/hardware/basys3_adapter.h"

#include <chrono>
#include <cmath>

namespace workbench::hardware {

MockBasys3Adapter::MockBasys3Adapter() {
    m_descriptor.device_id = "basys3-xc7a35t-01";
    m_descriptor.model_name = "Digilent Basys 3 (SIMULATED)";
    m_descriptor.manufacturer = "Digilent";
    m_descriptor.serial_number = "SIM-BASYS3-0084";
    m_descriptor.is_simulated = true;
    m_descriptor.firmware_version = "FT2232HQ Bridge v1.2 (SIMULATED)";
    m_descriptor.physical_interface = "Micro-USB (Onboard FT2232HQ JTAG/UART Bridge)";
    m_descriptor.type = DeviceType::FpgaBoard;

    m_capabilities.supports_swd = false;
    m_capabilities.supports_jtag = true;
    m_capabilities.supports_uart_telemetry = true;
    m_capabilities.supports_fpga_bitstream_flashing = true;
    m_capabilities.supports_mcu_firmware_flashing = false;
    m_capabilities.supports_voltage_monitoring = true;
    m_capabilities.nominal_logic_voltage_v = 3.3;
    m_capabilities.debug_protocol = "JTAG (IEEE 1149.1)";
    m_capabilities.connector_type = "Micro-USB (Onboard FT2232HQ) & 6-pin JTAG Footprint (J1)";
}

const DeviceDescriptor& MockBasys3Adapter::descriptor() const {
    return m_descriptor;
}

const DeviceCapabilities& MockBasys3Adapter::capabilities() const {
    return m_capabilities;
}

ConnectionState MockBasys3Adapter::state() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

core::Status MockBasys3Adapter::connect(const ConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (config.baud_rate == 0) {
        return core::Status::failure(
            core::StatusCode::InvalidInput,
            "Cannot connect Digilent Basys 3: baud rate must be greater than 0."
        );
    }

    if (config.timeout_ms == 0) {
        return core::Status::failure(
            core::StatusCode::InvalidInput,
            "Cannot connect Digilent Basys 3: timeout must be greater than 0 ms."
        );
    }

    m_config = config;
    if (config.simulate_timeout) {
        m_force_timeout = true;
    }
    if (config.simulate_connection_failure) {
        m_force_failure = true;
    }

    if (m_force_timeout) {
        m_state = ConnectionState::Error;
        return core::Status::failure(
            core::StatusCode::Timeout,
            "Simulated connection timeout: Basys 3 FT2232HQ USB-JTAG bridge failed to respond within " +
            std::to_string(config.timeout_ms) + " ms."
        );
    }

    if (m_force_failure) {
        m_state = ConnectionState::Error;
        return core::Status::failure(
            core::StatusCode::ConnectionFailed,
            "Simulated connection failure: FT2232HQ USB device open failed (SIMULATED)."
        );
    }

    m_state = ConnectionState::Connected;
    return core::Status::ok();
}

core::Status MockBasys3Adapter::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = ConnectionState::Disconnected;
    return core::Status::ok();
}

core::Result<TelemetryMessage> MockBasys3Adapter::read_telemetry() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != ConnectionState::Connected) {
        return core::Result<TelemetryMessage>::failure(
            core::StatusCode::DeviceDisconnected,
            "Cannot read telemetry: Digilent Basys 3 is not connected (current state: " +
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

    // Physically plausible Artix-7 XC7A35T rail values:
    // VCCINT = 1.00V nominal, VCCAUX = 1.80V, VCCO = 3.30V
    // Die temperature via XADC ~ 44.0 C, power ~ 1.25 W
    const double seq_offset = static_cast<double>(m_sequence_counter % 20);
    msg.core_voltage_v = 1.00 + 0.005 * std::sin(seq_offset * 0.314);      // VCCINT
    msg.auxiliary_voltage_v = 1.80 + 0.008 * std::cos(seq_offset * 0.314); // VCCAUX
    msg.board_temperature_c = 44.2 + 1.1 * std::sin(seq_offset * 0.4);     // XADC Die Temp
    msg.power_consumption_w = 1.25 + 0.05 * std::cos(seq_offset * 0.2);
    msg.status_flag = "FPGA_CONFIGURED_DONE (SIMULATED)";

    return core::Result<TelemetryMessage>::success(msg);
}

core::Status MockBasys3Adapter::execute_command(const std::string& command, const std::string& /*payload*/) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != ConnectionState::Connected) {
        return core::Status::failure(
            core::StatusCode::DeviceDisconnected,
            "Cannot execute command '" + command + "': Digilent Basys 3 is not connected."
        );
    }

    if (command == "ping") {
        return core::Status::ok();
    }
    if (command == "flash_bitstream") {
        return core::Status::ok();
    }
    if (command == "read_xadc") {
        return core::Status::ok();
    }
    if (command == "fpga_reset") {
        return core::Status::ok();
    }

    // Explicit unsupported capability rejections
    if (command == "flash_mcu_firmware") {
        return core::Status::failure(
            core::StatusCode::UnsupportedOperation,
            "Command 'flash_mcu_firmware' unsupported: Digilent Basys 3 is an FPGA development board, not an MCU."
        );
    }
    if (command == "read_arm_cortex_registers") {
        return core::Status::failure(
            core::StatusCode::UnsupportedOperation,
            "Command 'read_arm_cortex_registers' unsupported: Digilent Basys 3 Artix-7 does not contain a hard ARM Cortex core."
        );
    }

    return core::Status::failure(
        core::StatusCode::UnsupportedOperation,
        "Command '" + command + "' is not supported by Digilent Basys 3 adapter."
    );
}

void MockBasys3Adapter::reset_simulation_flags() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_force_timeout = false;
    m_force_failure = false;
}

} // namespace workbench::hardware
