#pragma once

#include <cstdint>
#include <string>

namespace workbench::performance {

/// @brief Holds hardware and runtime environment metadata detected on the host system.
struct SystemMetadata {
    std::string cpu_model = "Unavailable";
    uint32_t physical_core_count = 0;       // 0 indicates unavailable/unsupported
    uint32_t logical_processor_count = 1;   // Safe fallback to at least 1
    uint64_t total_ram_bytes = 0;
    std::string total_ram_readable = "Unavailable";
    std::string os_name = "Unavailable";
    std::string compiler_name_and_version = "Unavailable";
    std::string build_configuration = "Unknown";
    std::string cpp_standard = "Unknown";
    bool hardware_concurrency_fallback_used = false;
};

/// @brief Queries reliable hardware and compiler metadata from the host operating system.
/// @return SystemMetadata populated with detected capabilities.
SystemMetadata get_system_metadata();

/// @brief Formats a byte quantity into a human-readable string (e.g., "15.63 GB").
std::string format_bytes(uint64_t bytes);

} // namespace workbench::performance
