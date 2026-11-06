cmake_minimum_required(VERSION 3.20)

# -----------------------------------------------------------------------------
# Project Definition
# -----------------------------------------------------------------------------
project(UnifiedEngineeringWorkbench
    VERSION 1.0.0
    DESCRIPTION "Unified Engineering Workbench - High-Performance Engineering Workstation"
    LANGUAGES CXX
)

# -----------------------------------------------------------------------------
# C++ Standard Configuration
# -----------------------------------------------------------------------------
# Enforce C++17 standard for all targets
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable compiler-specific extensions to guarantee portable, standard C++17
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Target: workbench_core (Static Library)
# -----------------------------------------------------------------------------
add_library(workbench_core STATIC
    # Core Foundation
    src/core/workbench_core.h
    src/core/workbench_core.cpp
    src/core/result.h
    src/core/vector3.h
    src/core/bounding_box.h
    src/core/cancellation_token.h
    src/core/validation.h
    src/core/validation.cpp

    # Geometry & CSV Parsers (Stage 4)
    src/modules/geometry/stl_parser.h
    src/modules/geometry/stl_parser.cpp
    src/modules/geometry/csv_parser.h
    src/modules/geometry/csv_parser.cpp

    # Thermal FEM Solver (Stage 5)
    src/modules/thermal/fem_1d_solver.h
    src/modules/thermal/fem_1d_solver.cpp

    # Performance & Multicore Benchmarking (Stage 6)
    src/modules/performance/system_info.h
    src/modules/performance/system_info.cpp
    src/modules/performance/batch_generator.h
    src/modules/performance/batch_generator.cpp
    src/modules/performance/batch_runner.h
    src/modules/performance/batch_runner.cpp
    src/modules/performance/correctness_verifier.h
    src/modules/performance/correctness_verifier.cpp
    src/modules/performance/benchmark_runner.h
    src/modules/performance/benchmark_runner.cpp

    # Hardware Integration & Protocols (Stage 7)
    src/modules/hardware/hardware_types.h
    src/modules/hardware/hardware_adapter.h
    src/modules/hardware/stm32_adapter.h
    src/modules/hardware/stm32_adapter.cpp
    src/modules/hardware/basys3_adapter.h
    src/modules/hardware/basys3_adapter.cpp
    src/modules/hardware/jtag_hs2_adapter.h
    src/modules/hardware/jtag_hs2_adapter.cpp
    src/modules/hardware/hardware_manager.h
    src/modules/hardware/hardware_manager.cpp

    # Backpropagation & Numerical Verification (Stage 8)
    src/modules/backprop/network_types.h
    src/modules/backprop/backprop_engine.h
    src/modules/backprop/backprop_engine.cpp
)

# Expose 'src' directory so headers can be included cleanly as <core/...> and <modules/...>
target_include_directories(workbench_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Standardize warning levels and strict conformance across compilers
if(MSVC)
    target_compile_options(workbench_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(workbench_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# -----------------------------------------------------------------------------
# Target: workbench_smoke_test (Smoke Test Executable)
# -----------------------------------------------------------------------------
add_executable(workbench_smoke_test
    src/main.cpp
)

target_link_libraries(workbench_smoke_test PRIVATE
    workbench_core
)

if(MSVC)
    target_compile_options(workbench_smoke_test PRIVATE /W4 /permissive-)
else()
    target_compile_options(workbench_smoke_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# -----------------------------------------------------------------------------
# Testing Configuration & Test Runner Targets
# -----------------------------------------------------------------------------
enable_testing()

# 1. Core Module Unit Tests
add_executable(workbench_core_tests
    tests/test_core.cpp
)

target_link_libraries(workbench_core_tests PRIVATE
    workbench_core
)

if(MSVC)
    target_compile_options(workbench_core_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(workbench_core_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME WorkbenchCoreTests COMMAND workbench_core_tests)
set_tests_properties(WorkbenchCoreTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 2. Geometry & CSV Unit Tests (Stage 4)
add_executable(workbench_geometry_tests
    tests/test_geometry.cpp
)

target_link_libraries(workbench_geometry_tests PRIVATE
    workbench_core
)

if(MSVC)
    target_compile_options(workbench_geometry_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(workbench_geometry_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME WorkbenchGeometryTests COMMAND workbench_geometry_tests)
set_tests_properties(WorkbenchGeometryTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 3. Thermal FEM Unit Tests (Stage 5)
add_executable(workbench_thermal_tests
    tests/test_thermal.cpp
)

target_link_libraries(workbench_thermal_tests PRIVATE
    workbench_core
)

if(MSVC)
    target_compile_options(workbench_thermal_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(workbench_thermal_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME WorkbenchThermalTests COMMAND workbench_thermal_tests)
set_tests_properties(WorkbenchThermalTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 4. Performance & Multicore Benchmarking Unit Tests (Stage 6)
add_executable(workbench_performance_tests
    tests/test_performance.cpp
)

target_link_libraries(workbench_performance_tests PRIVATE
    workbench_core
)

if(MSVC)
    target_compile_options(workbench_performance_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(workbench_performance_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME WorkbenchPerformanceTests COMMAND workbench_performance_tests)
set_tests_properties(WorkbenchPerformanceTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 5. Hardware Integration Unit Tests (Stage 7)
add_executable(workbench_hardware_tests
    tests/test_hardware.cpp
)

target_link_libraries(workbench_hardware_tests PRIVATE
    workbench_core
)

if(MSVC)
    target_compile_options(workbench_hardware_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(workbench_hardware_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME WorkbenchHardwareTests COMMAND workbench_hardware_tests)
set_tests_properties(WorkbenchHardwareTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 6. Backpropagation Unit Tests (Stage 8)
add_executable(workbench_backprop_tests
    tests/test_backprop.cpp
)

target_link_libraries(workbench_backprop_tests PRIVATE
    workbench_core
)

if(MSVC)
    target_compile_options(workbench_backprop_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(workbench_backprop_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME WorkbenchBackpropTests COMMAND workbench_backprop_tests)
set_tests_properties(WorkbenchBackpropTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# -----------------------------------------------------------------------------
# Target: workbench_benchmark_cli (Task D Standalone Multicore Benchmark CLI)
# -----------------------------------------------------------------------------
add_executable(workbench_benchmark_cli
    src/benchmark_cli.cpp
)

target_link_libraries(workbench_benchmark_cli PRIVATE
    workbench_core
)

if(MSVC)
    target_compile_options(workbench_benchmark_cli PRIVATE /W4 /permissive-)
else()
    target_compile_options(workbench_benchmark_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# STAGE 9.1: DESKTOP UI FOUNDATION & APPLICATION SHELL
# =============================================================================

# -----------------------------------------------------------------------------
# Dependency: webview2_loader (Microsoft Edge WebView2 SDK)
# -----------------------------------------------------------------------------
add_library(webview2_loader INTERFACE)
target_include_directories(webview2_loader INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/include
)
target_link_libraries(webview2_loader INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/lib/x64/WebView2Loader.dll.lib"
)

# -----------------------------------------------------------------------------
# Target: workbench_ui (Desktop UI Host & Bridge Static Library)
# -----------------------------------------------------------------------------
add_library(workbench_ui STATIC
    src/ui/bridge/host_bridge.h
    src/ui/bridge/host_bridge.cpp
    src/ui/host/webview_host.h
    src/ui/host/webview_host.cpp
)

target_include_directories(workbench_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/include
)

target_link_libraries(workbench_ui PUBLIC
    workbench_core
    webview2_loader
    shlwapi
)

if(MSVC)
    target_compile_options(workbench_ui PRIVATE /W4 /permissive-)
else()
    target_compile_options(workbench_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()

# -----------------------------------------------------------------------------
# Target: workbench_ui_tests (UI Host & Bridge Headless Unit Tests)
# -----------------------------------------------------------------------------
add_executable(workbench_ui_tests
    tests/test_ui_host.cpp
)

target_link_libraries(workbench_ui_tests PRIVATE
    workbench_ui
    workbench_core
    webview2_loader
)

if(MSVC)
    target_compile_options(workbench_ui_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(workbench_ui_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME WorkbenchUITests COMMAND workbench_ui_tests)
set_tests_properties(WorkbenchUITests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Copy WebView2Loader.dll next to unit test runner
add_custom_command(TARGET workbench_ui_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/bin/x64/WebView2Loader.dll"
        "$<TARGET_FILE_DIR:workbench_ui_tests>"
    COMMENT "Copying WebView2Loader.dll next to workbench_ui_tests.exe"
)

# -----------------------------------------------------------------------------
# Target: workbench_desktop (Native Win32 + WebView2 Desktop Application)
# -----------------------------------------------------------------------------
add_executable(workbench_desktop WIN32
    src/ui/host/main_host.cpp
)

target_link_libraries(workbench_desktop PRIVATE
    workbench_ui
    workbench_core
    webview2_loader
    shlwapi
)

if(MSVC)
    target_compile_options(workbench_desktop PRIVATE /W4 /permissive-)
else()
    target_compile_options(workbench_desktop PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Post-build: Ensure runtime dependencies and frontend directory accompany executable
add_custom_command(TARGET workbench_desktop POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/bin/x64/WebView2Loader.dll"
        "$<TARGET_FILE_DIR:workbench_desktop>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/frontend"
        "$<TARGET_FILE_DIR:workbench_desktop>/frontend"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data/samples"
        "$<TARGET_FILE_DIR:workbench_desktop>/data/samples"
    COMMENT "Copying WebView2Loader.dll, frontend assets and sample datasets next to workbench_desktop.exe"
)

# =============================================================================
# STAGE 10: STANDALONE PACKAGING AND LAUNCH.EXE
# =============================================================================

# -----------------------------------------------------------------------------
# Target: Launch (Native Windows Desktop GUI Launcher)
# -----------------------------------------------------------------------------
add_executable(Launch WIN32
    src/launcher/main_launcher.cpp
)

if(MSVC)
    target_compile_options(Launch PRIVATE /W4 /permissive-)
else()
    target_compile_options(Launch PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Copy Launch.exe next to workbench_desktop.exe in build output
add_custom_command(TARGET Launch POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Launch>"
        "$<TARGET_FILE_DIR:workbench_desktop>/Launch.exe"
    COMMENT "Copying Launch.exe next to workbench_desktop.exe"
)

# -----------------------------------------------------------------------------
# Target: package_workbench (Standalone Runtime Package Staging & Archiving)
# -----------------------------------------------------------------------------
set(PACKAGE_STAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/package/UnifiedEngineeringWorkbench")

add_custom_target(package_workbench
    DEPENDS Launch workbench_desktop
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_STAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Launch>"
        "${PACKAGE_STAGE_DIR}/Launch.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:workbench_desktop>"
        "${PACKAGE_STAGE_DIR}/workbench_desktop.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/bin/x64/WebView2Loader.dll"
        "${PACKAGE_STAGE_DIR}/WebView2Loader.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/frontend"
        "${PACKAGE_STAGE_DIR}/frontend"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data/samples"
        "${PACKAGE_STAGE_DIR}/data/samples"
    COMMAND ${CMAKE_COMMAND} -E tar cf "UnifiedEngineeringWorkbench-1.0.0-win64.zip" --format=zip "UnifiedEngineeringWorkbench"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/package"
    COMMENT "Staging standalone runtime package in package/UnifiedEngineeringWorkbench and creating ZIP"
)


