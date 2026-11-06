#include "modules/performance/system_info.h"

#include <thread>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <algorithm>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#endif

namespace workbench::performance {

namespace {

#if defined(_WIN32)
std::string query_cpu_brand_string() {
    int cpu_info[4] = {0};
    __cpuid(cpu_info, static_cast<int>(0x80000000));
    const unsigned int nExIds = static_cast<unsigned int>(cpu_info[0]);

    if (nExIds < 0x80000004) {
        return "Unavailable";
    }

    char brand[49] = {0};
    __cpuid(reinterpret_cast<int*>(brand), static_cast<int>(0x80000002));
    __cpuid(reinterpret_cast<int*>(brand + 16), static_cast<int>(0x80000003));
    __cpuid(reinterpret_cast<int*>(brand + 32), static_cast<int>(0x80000004));

    std::string brand_str(brand);
    // Trim leading whitespace
    const auto start = brand_str.find_first_not_of(" \t\r\n");
    if (start != std::string::npos) {
        brand_str = brand_str.substr(start);
    }
    // Trim trailing whitespace
    const auto end = brand_str.find_last_not_of(" \t\r\n");
    if (end != std::string::npos) {
        brand_str = brand_str.substr(0, end + 1);
    }

    return brand_str.empty() ? "Unavailable" : brand_str;
}

uint32_t query_physical_core_count() {
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || length == 0) {
        return 0; // Unavailable
    }

    std::vector<uint8_t> buffer(length);
    auto pInfo = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());

    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, pInfo, &length)) {
        return 0; // Unavailable
    }

    uint32_t core_count = 0;
    DWORD offset = 0;
    while (offset < length) {
        auto current = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
        if (current->Relationship == RelationProcessorCore) {
            core_count++;
        }
        if (current->Size == 0) break;
        offset += current->Size;
    }

    return core_count;
}

std::string query_os_version() {
    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        auto rtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(hNtdll, "RtlGetVersion"));
        if (rtlGetVersion) {
            RTL_OSVERSIONINFOW rovi;
            std::memset(&rovi, 0, sizeof(rovi));
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (rtlGetVersion(&rovi) == 0) {
                std::ostringstream oss;
                if (rovi.dwMajorVersion == 10 && rovi.dwBuildNumber >= 22000) {
                    oss << "Microsoft Windows 11 (Build " << rovi.dwBuildNumber << ")";
                } else if (rovi.dwMajorVersion == 10) {
                    oss << "Microsoft Windows 10 (Build " << rovi.dwBuildNumber << ")";
                } else {
                    oss << "Microsoft Windows " << rovi.dwMajorVersion << "." << rovi.dwMinorVersion
                        << " (Build " << rovi.dwBuildNumber << ")";
                }
                return oss.str();
            }
        }
    }
    return "Microsoft Windows (Version Unknown)";
}

uint64_t query_total_ram() {
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    if (GlobalMemoryStatusEx(&mem_status)) {
        return mem_status.ullTotalPhys;
    }
    return 0;
}
#endif

} // namespace

std::string format_bytes(uint64_t bytes) {
    if (bytes == 0) return "Unavailable";
    const double gib = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << gib << " GB";
    return oss.str();
}

SystemMetadata get_system_metadata() {
    SystemMetadata meta;

    // 1. Logical processors with safe fallback
    const unsigned int hc = std::thread::hardware_concurrency();
    if (hc == 0) {
        meta.logical_processor_count = 1;
        meta.hardware_concurrency_fallback_used = true;
    } else {
        meta.logical_processor_count = hc;
        meta.hardware_concurrency_fallback_used = false;
    }

#if defined(_WIN32)
    // 2. CPU Model Name
    meta.cpu_model = query_cpu_brand_string();

    // 3. Physical core count
    meta.physical_core_count = query_physical_core_count();

    // 4. Total system RAM
    meta.total_ram_bytes = query_total_ram();
    meta.total_ram_readable = format_bytes(meta.total_ram_bytes);

    // 5. Operating System
    meta.os_name = query_os_version();
#else
    meta.cpu_model = "Unavailable (Non-Windows)";
    meta.physical_core_count = 0;
    meta.total_ram_bytes = 0;
    meta.total_ram_readable = "Unavailable";
    meta.os_name = "Non-Windows";
#endif

    // 6. Compiler Name and Version
#if defined(_MSC_VER)
    std::ostringstream comp_ss;
    comp_ss << "MSVC " << _MSC_VER;
#if defined(_MSC_FULL_VER)
    comp_ss << " (Full: " << _MSC_FULL_VER << ")";
#endif
    meta.compiler_name_and_version = comp_ss.str();
#elif defined(__clang__)
    meta.compiler_name_and_version = "Clang " __clang_version__;
#elif defined(__GNUC__)
    meta.compiler_name_and_version = "GCC " __VERSION__;
#else
    meta.compiler_name_and_version = "Unknown Compiler";
#endif

    // 7. Build Configuration
#if defined(NDEBUG)
    meta.build_configuration = "Release";
#else
    meta.build_configuration = "Debug";
#endif

    // 8. C++ Standard
#if defined(_MSVC_LANG)
    const long std_val = _MSVC_LANG;
#else
    const long std_val = __cplusplus;
#endif

    if (std_val >= 202002L) {
        meta.cpp_standard = "C++20";
    } else if (std_val >= 201703L) {
        meta.cpp_standard = "C++17";
    } else if (std_val >= 201402L) {
        meta.cpp_standard = "C++14";
    } else {
        meta.cpp_standard = "C++11 or earlier";
    }

    return meta;
}

} // namespace workbench::performance
