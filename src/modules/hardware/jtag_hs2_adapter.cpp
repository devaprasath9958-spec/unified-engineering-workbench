#include "modules/hardware/jtag_hs2_adapter.h"

#include <chrono>
#include <cmath>

namespace workbench::hardware {

MockJtagHs2Adapter::MockJtagHs2Adapter() {
    m_descriptor.device_id = "jtag-hs2-adapter-01";
    m_descriptor.model_name = "Digilent JTAG-HS2 (SIMULATED)";
    m_descriptor.manufacturer = "Digilent";
    m_descriptor.serial_number = "SIM-JTAG-HS2-0019";
    m_descriptor.is_simulated = true;
    m_descriptor.firmware_version = "FTDI MPSSE v2.1 (SIMULATED)";
    m_descriptor.physical_interface = "USB Type-A to 14-pin 2x7 2mm JTAG Ribbon Cable";
    m_descriptor.type = DeviceType::JtagAdapter;

    m_capabilities.supports_swd = false;
    m_capabilities.supports_jtag = true;
    m_capabilities.supports_uart_telemetry = false; // Verified fact: dedicated JTAG cable, no UART data channel
    m_capabilities.supports_fpga_bitstream_flashing = true; // Transport layer for bitstream shifting
    m_capabilities.supports_mcu_firmware_flashing = false;
    m_capabilities.supports_voltage_monitoring = true; // Target VREF sensing (1.8V to 5.0V)
    m_capabilities.nominal_logic_voltage_v = 3.3; // Sensed from VREF
    m_capabilities.debug_protocol = "JTAG (IEEE 1149.1)";
    m_capabilities.connector_type = "14-pin 2x7 2mm Ribbon Connector (Target VREF Input)";
}

const DeviceDescriptor& MockJtagHs2Adapter::descriptor() const {
    return m_descriptor;
}

const DeviceCapabilities& MockJtagHs2Adapter::capabilities() const {
    return m_capabilities;
}

ConnectionState MockJtagHs2Adapter::state() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

core::Status MockJtagHs2Adapter::connect(const ConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (config.baud_rate == 0) {
        return core::Status::failure(
            core::StatusCode::InvalidInput,
            "Cannot connect Digilent JTAG-HS2: baud rate / clock rate must be greater than 0."
        );
    }

    if (config.timeout_ms == 0) {
        return core::Status::failure(
            core::StatusCode::InvalidInput,
            "Cannot connect Digilent JTAG-HS2: timeout must be greater than 0 ms."
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
            "Simulated connection timeout: JTAG-HS2 USB endpoint failed to respond within " +
            std::to_string(config.timeout_ms) + " ms."
        );
    }

    if (m_force_failure) {
        m_state = ConnectionState::Error;
        return core::Status::failure(
            core::StatusCode::ConnectionFailed,
            "Simulated connection failure: JTAG-HS2 target VREF voltage not detected (SIMULATED)."
        );
    }

    m_state = ConnectionState::Connected;
    return core::Status::ok();
}

core::Status MockJtagHs2Adapter::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = ConnectionState::Disconnected;
    return core::Status::ok();
}

core::Result<TelemetryMessage> MockJtagHs2Adapter::read_telemetry() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != ConnectionState::Connected) {
        return core::Result<TelemetryMessage>::failure(
            core::StatusCode::DeviceDisconnected,
            "Cannot read telemetry: Digilent JTAG-HS2 is not connected (current state: " +
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

    // JTAG-HS2 values:
    // Target VREF sense input = 3.30V (level translation reference, NOT power supply)
    // Internal 3.3V LDO = 3.30V
    // Operating temperature = 28.5 C, power = 0.40 W
    const double seq_offset = static_cast<double>(m_sequence_counter % 20);
    msg.core_voltage_v = 3.30 + 0.005 * std::sin(seq_offset * 0.314);      // Sensed Target VREF
    msg.auxiliary_voltage_v = 3.30 + 0.002 * std::cos(seq_offset * 0.314); // Internal LDO
    msg.board_temperature_c = 28.5 + 0.4 * std::sin(seq_offset * 0.5);     // Adapter enclosure temp
    msg.power_consumption_w = 0.40 + 0.01 * std::cos(seq_offset * 0.2);
    msg.status_flag = "TCK_30MHZ_ACTIVE (SIMULATED)";

    return core::Result<TelemetryMessage>::success(msg);
}

core::Status MockJtagHs2Adapter::execute_command(const std::string& command, const std::string& /*payload*/) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != ConnectionState::Connected) {
        return core::Status::failure(
            core::StatusCode::DeviceDisconnected,
            "Cannot execute command '" + command + "': Digilent JTAG-HS2 is not connected."
        );
    }

    if (command == "ping") {
        return core::Status::ok();
    }
    if (command == "jtag_scan_chain") {
        return core::Status::ok();
    }
    if (command == "set_tck_frequency") {
        return core::Status::ok();
    }
    if (command == "jtag_shift_dr") {
        return core::Status::ok();
    }
    if (command == "jtag_shift_ir") {
        return core::Status::ok();
    }

    // Explicit unsupported capability rejections
    if (command == "read_uart_telemetry") {
        return core::Status::failure(
            core::StatusCode::UnsupportedOperation,
            "Command 'read_uart_telemetry' unsupported: Digilent JTAG-HS2 is a dedicated JTAG cable and does not provide a UART channel."
        );
    }
    if (command == "flash_mcu_firmware") {
        return core::Status::failure(
            core::StatusCode::UnsupportedOperation,
            "Command 'flash_mcu_firmware' unsupported: Digilent JTAG-HS2 is a JTAG programming cable, not an MCU programmer."
        );
    }

    return core::Status::failure(
        core::StatusCode::UnsupportedOperation,
        "Command '" + command + "' is not supported by Digilent JTAG-HS2 adapter."
    );
}

void MockJtagHs2Adapter::reset_simulation_flags() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_force_timeout = false;
    m_force_failure = false;
}

} // namespace workbench::hardware
