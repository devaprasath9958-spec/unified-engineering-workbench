#include "modules/hardware/hardware_types.h"
#include "modules/hardware/hardware_adapter.h"
#include "modules/hardware/hardware_manager.h"
#include "modules/hardware/stm32_adapter.h"
#include "modules/hardware/basys3_adapter.h"
#include "modules/hardware/jtag_hs2_adapter.h"

#include <iostream>
#include <cmath>
#include <thread>
#include <vector>
#include <set>

using namespace workbench;
using namespace workbench::hardware;

static int g_hw_passed = 0;
static int g_hw_failed = 0;

#define HW_ASSERT(cond, msg) \
    do { \
        if (cond) { \
            g_hw_passed++; \
        } else { \
            g_hw_failed++; \
            std::cerr << "  [FAIL] Line " << __LINE__ << ": " << (msg) << "\n"; \
        } \
    } while (0)

// 1. Discovery of all three simulated devices
void test_device_discovery() {
    std::cout << "[TEST GROUP 1] Discovery of All Three Simulated Devices...\n";

    auto manager = create_default_hardware_manager();
    const auto devices = manager->discover_devices();

    HW_ASSERT(devices.size() == 3, "HardwareManager discovers exactly 3 target devices");

    std::set<std::string> found_ids;
    for (const auto& dev : devices) {
        found_ids.insert(dev.device_id);
        HW_ASSERT(dev.is_simulated == true, "Device explicitly marked is_simulated == true");
    }

    HW_ASSERT(found_ids.count("stm32f407g-disc1-01") == 1, "Discovered STM32F407G-DISC1");
    HW_ASSERT(found_ids.count("basys3-xc7a35t-01") == 1, "Discovered Digilent Basys 3");
    HW_ASSERT(found_ids.count("jtag-hs2-adapter-01") == 1, "Discovered Digilent JTAG-HS2");
}

// 2. Device capability reporting
void test_device_capabilities() {
    std::cout << "[TEST GROUP 2] Device Capability Reports...\n";

    auto manager = create_default_hardware_manager();

    // STM32 Capabilities
    const auto stm32 = manager->get_adapter("stm32f407g-disc1-01");
    HW_ASSERT(stm32 != nullptr, "STM32 adapter retrieved");
    const auto& stm_cap = stm32->capabilities();
    HW_ASSERT(stm_cap.supports_swd == true, "STM32 supports SWD");
    HW_ASSERT(stm_cap.supports_jtag == false, "STM32 onboard ST-LINK uses SWD; no 20-pin JTAG");
    HW_ASSERT(stm_cap.supports_mcu_firmware_flashing == true, "STM32 supports MCU firmware flashing");
    HW_ASSERT(stm_cap.supports_fpga_bitstream_flashing == false, "STM32 rejects FPGA flashing capability");

    // Basys 3 Capabilities
    const auto basys3 = manager->get_adapter("basys3-xc7a35t-01");
    HW_ASSERT(basys3 != nullptr, "Basys 3 adapter retrieved");
    const auto& basys_cap = basys3->capabilities();
    HW_ASSERT(basys_cap.supports_swd == false, "Basys 3 does not support SWD");
    HW_ASSERT(basys_cap.supports_jtag == true, "Basys 3 supports JTAG");
    HW_ASSERT(basys_cap.supports_fpga_bitstream_flashing == true, "Basys 3 supports FPGA bitstream flashing");
    HW_ASSERT(basys_cap.supports_mcu_firmware_flashing == false, "Basys 3 rejects MCU firmware flashing");

    // JTAG-HS2 Capabilities
    const auto hs2 = manager->get_adapter("jtag-hs2-adapter-01");
    HW_ASSERT(hs2 != nullptr, "JTAG-HS2 adapter retrieved");
    const auto& hs2_cap = hs2->capabilities();
    HW_ASSERT(hs2_cap.supports_jtag == true, "JTAG-HS2 supports JTAG");
    HW_ASSERT(hs2_cap.supports_uart_telemetry == false, "JTAG-HS2 is dedicated JTAG; no UART channel");
    HW_ASSERT(hs2_cap.supports_voltage_monitoring == true, "JTAG-HS2 supports target VREF level monitoring");
}

// 3. Successful connection and disconnection
void test_connection_lifecycle() {
    std::cout << "[TEST GROUP 3] Connection and Disconnection Lifecycle...\n";

    auto manager = create_default_hardware_manager();

    // Check initial state
    HW_ASSERT(manager->get_device_state("stm32f407g-disc1-01").value() == ConnectionState::Disconnected,
              "Initial state is Disconnected");

    // Connect
    const auto conn_status = manager->connect_device("stm32f407g-disc1-01");
    HW_ASSERT(conn_status.is_ok(), "Connecting STM32 succeeds");
    HW_ASSERT(manager->get_device_state("stm32f407g-disc1-01").value() == ConnectionState::Connected,
              "State transitioned to Connected");

    // Disconnect
    const auto disc_status = manager->disconnect_device("stm32f407g-disc1-01");
    HW_ASSERT(disc_status.is_ok(), "Disconnecting STM32 succeeds");
    HW_ASSERT(manager->get_device_state("stm32f407g-disc1-01").value() == ConnectionState::Disconnected,
              "State transitioned to Disconnected");

    // Connect Basys 3 and JTAG-HS2
    HW_ASSERT(manager->connect_device("basys3-xc7a35t-01").is_ok(), "Basys 3 connect succeeds");
    HW_ASSERT(manager->connect_device("jtag-hs2-adapter-01").is_ok(), "JTAG-HS2 connect succeeds");
    HW_ASSERT(manager->connected_device_ids().size() == 2, "Exactly 2 devices connected");

    manager->disconnect_all();
    HW_ASSERT(manager->connected_device_ids().empty(), "All devices disconnected cleanly");
}

// 4. Telemetry generation & physical plausibility
void test_telemetry_generation() {
    std::cout << "[TEST GROUP 4] Telemetry Generation & Physical Plausibility...\n";

    auto manager = create_default_hardware_manager();

    // Reading when disconnected should fail
    const auto bad_telem = manager->read_device_telemetry("stm32f407g-disc1-01");
    HW_ASSERT(bad_telem.is_error(), "Reading telemetry while disconnected returns error");
    HW_ASSERT(bad_telem.error().code == core::StatusCode::DeviceDisconnected,
              "Error code is DeviceDisconnected");

    // Connect STM32 and read telemetry packets
    HW_ASSERT(manager->connect_device("stm32f407g-disc1-01").is_ok(), "STM32 connect succeeds");

    const auto t1 = manager->read_device_telemetry("stm32f407g-disc1-01");
    const auto t2 = manager->read_device_telemetry("stm32f407g-disc1-01");

    HW_ASSERT(t1.is_ok(), "First telemetry read succeeds");
    HW_ASSERT(t2.is_ok(), "Second telemetry read succeeds");

    const auto& msg1 = t1.value();
    const auto& msg2 = t2.value();

    HW_ASSERT(msg1.is_simulated == true, "Telemetry packet explicitly labelled is_simulated == true");
    HW_ASSERT(msg2.sequence_number == msg1.sequence_number + 1, "Sequence number increments monotonically");
    HW_ASSERT(msg2.timestamp_ms >= msg1.timestamp_ms, "Timestamp is monotonic non-decreasing");

    // Check physical bounds and finite values
    HW_ASSERT(std::isfinite(msg1.core_voltage_v), "Core voltage is finite");
    HW_ASSERT(std::isfinite(msg1.auxiliary_voltage_v), "Aux voltage is finite");
    HW_ASSERT(std::isfinite(msg1.board_temperature_c), "Temperature is finite");
    HW_ASSERT(std::isfinite(msg1.power_consumption_w), "Power is finite");

    HW_ASSERT(msg1.core_voltage_v >= 1.15 && msg1.core_voltage_v <= 1.25, "STM32 VCORE plausible (~1.2V)");
    HW_ASSERT(msg1.auxiliary_voltage_v >= 3.25 && msg1.auxiliary_voltage_v <= 3.35, "STM32 VDD plausible (~3.3V)");
    HW_ASSERT(msg1.board_temperature_c >= 30.0 && msg1.board_temperature_c <= 60.0, "STM32 Temp plausible (30-60 C)");

    // Read Basys 3 Telemetry
    HW_ASSERT(manager->connect_device("basys3-xc7a35t-01").is_ok(), "Basys 3 connect succeeds");
    const auto basys_t = manager->read_device_telemetry("basys3-xc7a35t-01");
    HW_ASSERT(basys_t.is_ok(), "Basys 3 telemetry read succeeds");
    HW_ASSERT(basys_t.value().core_voltage_v >= 0.95 && basys_t.value().core_voltage_v <= 1.05,
              "Artix-7 VCCINT plausible (~1.0V)");
    HW_ASSERT(basys_t.value().auxiliary_voltage_v >= 1.75 && basys_t.value().auxiliary_voltage_v <= 1.85,
              "Artix-7 VCCAUX plausible (~1.8V)");

    // Read JTAG-HS2 Telemetry
    HW_ASSERT(manager->connect_device("jtag-hs2-adapter-01").is_ok(), "JTAG-HS2 connect succeeds");
    const auto hs2_t = manager->read_device_telemetry("jtag-hs2-adapter-01");
    HW_ASSERT(hs2_t.is_ok(), "JTAG-HS2 telemetry read succeeds");
    HW_ASSERT(hs2_t.value().core_voltage_v >= 3.20 && hs2_t.value().core_voltage_v <= 3.40,
              "JTAG-HS2 target VREF level sensed (~3.3V)");
}

// 5. Unsupported operations rejection
void test_unsupported_operations() {
    std::cout << "[TEST GROUP 5] Unsupported Command Rejection...\n";

    auto manager = create_default_hardware_manager();
    manager->connect_device("stm32f407g-disc1-01");
    manager->connect_device("basys3-xc7a35t-01");
    manager->connect_device("jtag-hs2-adapter-01");

    // Supported commands succeed
    HW_ASSERT(manager->execute_device_command("stm32f407g-disc1-01", "ping").is_ok(), "STM32 ping succeeds");
    HW_ASSERT(manager->execute_device_command("stm32f407g-disc1-01", "flash_firmware").is_ok(), "STM32 flash succeeds");
    HW_ASSERT(manager->execute_device_command("basys3-xc7a35t-01", "flash_bitstream").is_ok(), "Basys 3 bitstream succeeds");
    HW_ASSERT(manager->execute_device_command("jtag-hs2-adapter-01", "jtag_scan_chain").is_ok(), "JTAG-HS2 scan succeeds");

    // Unsupported commands rejected with StatusCode::UnsupportedOperation
    const auto stm_bad = manager->execute_device_command("stm32f407g-disc1-01", "flash_fpga_bitstream");
    HW_ASSERT(stm_bad.is_error(), "STM32 rejects FPGA bitstream flashing");
    HW_ASSERT(stm_bad.error().code == core::StatusCode::UnsupportedOperation, "Error code is UnsupportedOperation");

    const auto basys_bad = manager->execute_device_command("basys3-xc7a35t-01", "read_arm_cortex_registers");
    HW_ASSERT(basys_bad.is_error(), "Basys 3 rejects ARM Cortex register access");
    HW_ASSERT(basys_bad.error().code == core::StatusCode::UnsupportedOperation, "Error code is UnsupportedOperation");

    const auto hs2_bad = manager->execute_device_command("jtag-hs2-adapter-01", "read_uart_telemetry");
    HW_ASSERT(hs2_bad.is_error(), "JTAG-HS2 rejects UART telemetry");
    HW_ASSERT(hs2_bad.error().code == core::StatusCode::UnsupportedOperation, "Error code is UnsupportedOperation");
}

// 6. Timeout and recoverable connection failure
void test_timeout_and_recoverable_failure() {
    std::cout << "[TEST GROUP 6] Timeout and Recoverable Failure Handling...\n";

    auto manager = create_default_hardware_manager();

    // 1. Injected Timeout
    ConnectionConfig timeout_cfg;
    timeout_cfg.simulate_timeout = true;
    timeout_cfg.timeout_ms = 250;

    const auto timeout_res = manager->connect_device("stm32f407g-disc1-01", timeout_cfg);
    HW_ASSERT(timeout_res.is_error(), "Simulated timeout returns failure");
    HW_ASSERT(timeout_res.error().code == core::StatusCode::Timeout, "Error code is Timeout");
    HW_ASSERT(manager->get_device_state("stm32f407g-disc1-01").value() == ConnectionState::Error,
              "State transitioned to Error upon timeout");

    // Safe recovery after timeout
    auto stm_adapter = manager->get_adapter("stm32f407g-disc1-01");
    stm_adapter->reset_simulation_flags();

    const auto recover_res = manager->connect_device("stm32f407g-disc1-01");
    HW_ASSERT(recover_res.is_ok(), "Subsequent connection succeeds cleanly (safe recovery)");
    HW_ASSERT(manager->get_device_state("stm32f407g-disc1-01").value() == ConnectionState::Connected,
              "Device state successfully recovered to Connected");
    manager->disconnect_device("stm32f407g-disc1-01");

    // 2. Injected Link Failure
    ConnectionConfig fail_cfg;
    fail_cfg.simulate_connection_failure = true;

    const auto fail_res = manager->connect_device("basys3-xc7a35t-01", fail_cfg);
    HW_ASSERT(fail_res.is_error(), "Simulated connection failure returns error");
    HW_ASSERT(fail_res.error().code == core::StatusCode::ConnectionFailed, "Error code is ConnectionFailed");
    HW_ASSERT(manager->get_device_state("basys3-xc7a35t-01").value() == ConnectionState::Error,
              "State transitioned to Error upon link failure");

    // Safe recovery after link failure
    auto basys_adapter = manager->get_adapter("basys3-xc7a35t-01");
    basys_adapter->reset_simulation_flags();

    const auto recover_basys = manager->connect_device("basys3-xc7a35t-01");
    HW_ASSERT(recover_basys.is_ok(), "Basys 3 recovered and connected successfully");
    HW_ASSERT(manager->get_device_state("basys3-xc7a35t-01").value() == ConnectionState::Connected,
              "Basys 3 state is Connected");
}

// 7. Invalid configurations rejection
void test_invalid_configurations() {
    std::cout << "[TEST GROUP 7] Invalid Configurations & Nonexistent Devices...\n";

    auto manager = create_default_hardware_manager();

    // Zero baud rate
    ConnectionConfig bad_baud;
    bad_baud.baud_rate = 0;
    const auto bad_baud_res = manager->connect_device("stm32f407g-disc1-01", bad_baud);
    HW_ASSERT(bad_baud_res.is_error(), "Zero baud rate rejected");
    HW_ASSERT(bad_baud_res.error().code == core::StatusCode::InvalidInput, "Error code is InvalidInput");

    // Zero timeout
    ConnectionConfig bad_timeout;
    bad_timeout.timeout_ms = 0;
    const auto bad_timeout_res = manager->connect_device("stm32f407g-disc1-01", bad_timeout);
    HW_ASSERT(bad_timeout_res.is_error(), "Zero timeout rejected");
    HW_ASSERT(bad_timeout_res.error().code == core::StatusCode::InvalidInput, "Error code is InvalidInput");

    // Nonexistent device ID
    const auto no_dev_res = manager->connect_device("non_existent_device_id_99");
    HW_ASSERT(no_dev_res.is_error(), "Connecting nonexistent device returns error");
    HW_ASSERT(no_dev_res.error().code == core::StatusCode::DeviceNotFound, "Error code is DeviceNotFound");
}

// 8. Correct simulated-device identification
void test_simulated_identification() {
    std::cout << "[TEST GROUP 8] Explicit Simulated Device Identification...\n";

    auto manager = create_default_hardware_manager();
    const auto devices = manager->discover_devices();

    for (const auto& dev : devices) {
        HW_ASSERT(dev.is_simulated == true, "is_simulated flag is strictly true");
        HW_ASSERT(dev.model_name.find("(SIMULATED)") != std::string::npos,
                  "Model name contains explicit (SIMULATED) label");
        HW_ASSERT(dev.serial_number.rfind("SIM-", 0) == 0,
                  "Serial number begins with SIM- prefix");
    }
}

// 9. Connection state consistency & state queries
void test_connection_state_consistency() {
    std::cout << "[TEST GROUP 9] State Machine Consistency Across Operations...\n";

    auto manager = create_default_hardware_manager();

    HW_ASSERT(manager->get_device_state("jtag-hs2-adapter-01").value() == ConnectionState::Disconnected,
              "Starts Disconnected");
    manager->connect_device("jtag-hs2-adapter-01");
    HW_ASSERT(manager->get_device_state("jtag-hs2-adapter-01").value() == ConnectionState::Connected,
              "Transitions to Connected");
    manager->disconnect_device("jtag-hs2-adapter-01");
    HW_ASSERT(manager->get_device_state("jtag-hs2-adapter-01").value() == ConnectionState::Disconnected,
              "Transitions back to Disconnected");
}

// 10. Thread safety and concurrent manager operations
void test_thread_safety() {
    std::cout << "[TEST GROUP 10] Thread-Safe Concurrent Device Operations...\n";

    auto manager = create_default_hardware_manager();
    manager->connect_device("stm32f407g-disc1-01");
    manager->connect_device("basys3-xc7a35t-01");
    manager->connect_device("jtag-hs2-adapter-01");

    std::vector<std::thread> threads;
    std::atomic<bool> all_threads_ok{true};

    for (int t = 0; t < 6; ++t) {
        threads.emplace_back([&manager, &all_threads_ok, t]() {
            const std::string dev_id = (t % 3 == 0) ? "stm32f407g-disc1-01" :
                                       (t % 3 == 1) ? "basys3-xc7a35t-01" : "jtag-hs2-adapter-01";
            for (int i = 0; i < 20; ++i) {
                const auto res = manager->read_device_telemetry(dev_id);
                if (res.is_error() || !res.value().is_simulated) {
                    all_threads_ok = false;
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    HW_ASSERT(all_threads_ok.load(), "Concurrent multi-threaded telemetry reads succeeded without collision");
    manager->disconnect_all();
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  UNIFIED ENGINEERING WORKBENCH TESTS  \n";
    std::cout << "  Stage 7: Hardware & Protocols (Task E)\n";
    std::cout << "========================================\n\n";

    test_device_discovery();
    test_device_capabilities();
    test_connection_lifecycle();
    test_telemetry_generation();
    test_unsupported_operations();
    test_timeout_and_recoverable_failure();
    test_invalid_configurations();
    test_simulated_identification();
    test_connection_state_consistency();
    test_thread_safety();

    std::cout << "\n========================================\n";
    std::cout << "  HARDWARE TESTS SUMMARY: " << g_hw_passed << " PASSED, " << g_hw_failed << " FAILED\n";
    std::cout << "========================================\n";

    return (g_hw_failed == 0) ? 0 : 1;
}
