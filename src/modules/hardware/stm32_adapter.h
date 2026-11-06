#pragma once

#include "modules/hardware/hardware_adapter.h"

#include <mutex>
#include <cstdint>

namespace workbench::hardware {

/// @brief Mock adapter simulating the STMicroelectronics STM32F407G-DISC1 Discovery Board.
class MockStm32DiscoveryAdapter : public IHardwareAdapter {
public:
    MockStm32DiscoveryAdapter();
    ~MockStm32DiscoveryAdapter() override = default;

    const DeviceDescriptor& descriptor() const override;
    const DeviceCapabilities& capabilities() const override;
    ConnectionState state() const override;

    core::Status connect(const ConnectionConfig& config = {}) override;
    core::Status disconnect() override;
    core::Result<TelemetryMessage> read_telemetry() override;
    core::Status execute_command(const std::string& command, const std::string& payload = "") override;
    void reset_simulation_flags() override;

private:
    DeviceDescriptor m_descriptor;
    DeviceCapabilities m_capabilities;
    ConnectionState m_state = ConnectionState::Disconnected;
    ConnectionConfig m_config;

    mutable std::mutex m_mutex;
    uint32_t m_sequence_counter = 0;
    bool m_force_timeout = false;
    bool m_force_failure = false;
};

} // namespace workbench::hardware
