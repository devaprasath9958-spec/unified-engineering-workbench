#include "core/workbench_core.h"

namespace workbench::core {

std::string get_application_name() {
    return "UNIFIED ENGINEERING WORKBENCH";
}

std::string get_version_string() {
    return "1.0.0";
}

bool initialize_core() {
    // Basic runtime environment check
    return true;
}

} // namespace workbench::core
