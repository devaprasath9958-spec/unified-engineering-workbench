#pragma once

#include <string>

namespace workbench::core {

/**
 * @brief Retrieves the formal display name of the application.
 * 
 * @return std::string The application name.
 */
std::string get_application_name();

/**
 * @brief Retrieves the semantic version string of the core engineering workbench.
 * 
 * @return std::string Version in semantic format (e.g. "1.0.0").
 */
std::string get_version_string();

/**
 * @brief Initializes the core engineering subsystem.
 * 
 * Validates fundamental math and environment integrity.
 * 
 * @return true if initialization succeeded, false otherwise.
 */
bool initialize_core();

} // namespace workbench::core
