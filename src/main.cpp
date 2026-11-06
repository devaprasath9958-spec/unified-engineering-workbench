#include <iostream>
#include "core/workbench_core.h"

int main() {
    // Retrieve application title from the workbench_core library
    const std::string app_name = workbench::core::get_application_name();

    // Print welcome banner and environment status
    std::cout << "========================================\n";
    std::cout << "     " << app_name << "\n";
    std::cout << "========================================\n\n";
    std::cout << "C++17 environment initialized successfully.\n\n";
    std::cout << "Project foundation is ready!\n";

    return 0;
}
