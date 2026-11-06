#include "modules/hardware/hardware_manager.h"
#include "modules/hardware/stm32_adapter.h"
#include "modules/hardware/basys3_adapter.h"
#include "modules/hardware/jtag_hs2_adapter.h"

namespace workbench::hardware {

HardwareManager::~HardwareManager() {
    disconnect_all();
}

void HardwareManager::register_adapter(std::shared_ptr<IHardwareAdapter> adapter) {
    if (!adapter) return;
    std::lock_guard<std::mutex> lock(m_registry_mutex);
    m_adapters[adapter->descriptor().device_id] = std::move(adapter);
}

std::vector<DeviceDescriptor> HardwareManager::discover_devices() const {
    std::lock_guard<std::mutex> lock(m_registry_mutex);
    std::vector<DeviceDescriptor> list;
    list.reserve(m_adapters.size());
    for (const auto& [id, adapter] : m_adapters) {
        if (adapter) {
            list.push_back(adapter->descriptor());
        }
    }
    return list;
}

std::shared_ptr<IHardwareAdapter> HardwareManager::get_adapter(const std::string& device_id) const {
    std::lock_guard<std::mutex> lock(m_registry_mutex);
    const auto it = m_adapters.find(device_id);
    if (it != m_adapters.end()) {
        return it->second;
    }
    return nullptr;
}

core::Status HardwareManager::connect_device(const std::string& device_id, const ConnectionConfig& config) {
    std::shared_ptr<IHardwareAdapter> adapter;
    {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        const auto it = m_adapters.find(device_id);
        if (it == m_adapters.end()) {
            return core::Status::failure(
                core::StatusCode::DeviceNotFound,
                "Device with ID '" + device_id + "' not found in hardware manager registry."
            );
        }
        adapter = it->second;
    }

    // Call connect outside of registry lock to allow concurrent operations on other devices
    return adapter->connect(config);
}

core::Status HardwareManager::disconnect_device(const std::string& device_id) {
    std::shared_ptr<IHardwareAdapter> adapter;
    {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        const auto it = m_adapters.find(device_id);
        if (it == m_adapters.end()) {
            return core::Status::failure(
                core::StatusCode::DeviceNotFound,
                "Device with ID '" + device_id + "' not found in hardware manager registry."
            );
        }
        adapter = it->second;
    }

    return adapter->disconnect();
}

core::Result<TelemetryMessage> HardwareManager::read_device_telemetry(const std::string& device_id) {
    std::shared_ptr<IHardwareAdapter> adapter;
    {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        const auto it = m_adapters.find(device_id);
        if (it == m_adapters.end()) {
            return core::Result<TelemetryMessage>::failure(
                core::StatusCode::DeviceNotFound,
                "Device with ID '" + device_id + "' not found in hardware manager registry."
            );
        }
        adapter = it->second;
    }

    return adapter->read_telemetry();
}

core::Status HardwareManager::execute_device_command(
    const std::string& device_id,
    const std::string& command,
    const std::string& payload
) {
    std::shared_ptr<IHardwareAdapter> adapter;
    {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        const auto it = m_adapters.find(device_id);
        if (it == m_adapters.end()) {
            return core::Status::failure(
                core::StatusCode::DeviceNotFound,
                "Device with ID '" + device_id + "' not found in hardware manager registry."
            );
        }
        adapter = it->second;
    }

    return adapter->execute_command(command, payload);
}

core::Result<ConnectionState> HardwareManager::get_device_state(const std::string& device_id) const {
    std::shared_ptr<IHardwareAdapter> adapter;
    {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        const auto it = m_adapters.find(device_id);
        if (it == m_adapters.end()) {
            return core::Result<ConnectionState>::failure(
                core::StatusCode::DeviceNotFound,
                "Device with ID '" + device_id + "' not found in hardware manager registry."
            );
        }
        adapter = it->second;
    }

    return core::Result<ConnectionState>::success(adapter->state());
}

std::vector<std::string> HardwareManager::connected_device_ids() const {
    std::vector<std::shared_ptr<IHardwareAdapter>> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        snapshot.reserve(m_adapters.size());
        for (const auto& [id, adapter] : m_adapters) {
            if (adapter) {
                snapshot.push_back(adapter);
            }
        }
    }

    std::vector<std::string> connected;
    for (const auto& adapter : snapshot) {
        if (adapter->state() == ConnectionState::Connected) {
            connected.push_back(adapter->descriptor().device_id);
        }
    }
    return connected;
}

void HardwareManager::disconnect_all() {
    std::vector<std::shared_ptr<IHardwareAdapter>> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        snapshot.reserve(m_adapters.size());
        for (const auto& [id, adapter] : m_adapters) {
            if (adapter) {
                snapshot.push_back(adapter);
            }
        }
    }

    for (const auto& adapter : snapshot) {
        adapter->disconnect();
    }
}

std::unique_ptr<HardwareManager> create_default_hardware_manager() {
    auto mgr = std::make_unique<HardwareManager>();
    mgr->register_adapter(std::make_shared<MockStm32DiscoveryAdapter>());
    mgr->register_adapter(std::make_shared<MockBasys3Adapter>());
    mgr->register_adapter(std::make_shared<MockJtagHs2Adapter>());
    return mgr;
}

} // namespace workbench::hardware
