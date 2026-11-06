#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <string>
#include <cwchar>

namespace {

const wchar_t kAppTitle[] = L"Unified Engineering Workbench";
const wchar_t kDesktopExeName[] = L"workbench_desktop.exe";
const wchar_t kLoaderDllName[] = L"WebView2Loader.dll";
const wchar_t kFrontendRelPath[] = L"frontend\\index.html";

void show_error_dialog(const std::wstring& message) {
    MessageBoxW(
        nullptr,
        message.c_str(),
        kAppTitle,
        MB_ICONERROR | MB_OK | MB_SETFOREGROUND | MB_TOPMOST
    );
}

std::wstring get_last_error_message(DWORD error_code) {
    LPWSTR buffer = nullptr;
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr
    );
    std::wstring message;
    if (size > 0 && buffer != nullptr) {
        message = buffer;
        LocalFree(buffer);
        // Trim trailing newlines
        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
            message.pop_back();
        }
    } else {
        message = L"Unknown system error code: " + std::to_wstring(error_code);
    }
    return message;
}

} // namespace

int WINAPI wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPWSTR lpCmdLine, int nCmdShow) {
    // 1. Determine launcher's own directory at runtime
    wchar_t module_path[MAX_PATH * 2] = {0};
    DWORD len = GetModuleFileNameW(nullptr, module_path, static_cast<DWORD>(sizeof(module_path) / sizeof(module_path[0])));
    if (len == 0 || len >= sizeof(module_path) / sizeof(module_path[0])) {
        show_error_dialog(L"Critical failure: Unable to determine application module path.");
        return 1;
    }

    std::filesystem::path launcher_dir = std::filesystem::path(module_path).parent_path();

    // 2. Resolve target application components relative to Launch.exe
    std::filesystem::path target_exe = launcher_dir / kDesktopExeName;
    std::filesystem::path loader_dll = launcher_dir / kLoaderDllName;
    std::filesystem::path frontend_index = launcher_dir / kFrontendRelPath;

    // 3. Verify presence of required runtime files
    std::error_code ec;
    if (!std::filesystem::exists(target_exe, ec)) {
        std::wstring err = L"Unified Engineering Workbench could not be launched.\n\n"
                           L"Target application executable was not found:\n" +
                           target_exe.wstring() + L"\n\n"
                           L"Please extract all files from the release archive before running Launch.exe.";
        show_error_dialog(err);
        return 2;
    }

    if (!std::filesystem::exists(loader_dll, ec)) {
        std::wstring err = L"Unified Engineering Workbench could not be launched.\n\n"
                           L"Required runtime dependency was not found:\n" +
                           loader_dll.wstring() + L"\n\n"
                           L"Please ensure WebView2Loader.dll resides in the same directory as Launch.exe.";
        show_error_dialog(err);
        return 3;
    }

    if (!std::filesystem::exists(frontend_index, ec)) {
        std::wstring err = L"Unified Engineering Workbench could not be launched.\n\n"
                           L"User interface assets directory is missing or incomplete:\n" +
                           frontend_index.wstring() + L"\n\n"
                           L"Please verify that the 'frontend' folder is present in the application directory.";
        show_error_dialog(err);
        return 4;
    }

    // 4. Construct command line with full quotation and forward any arguments
    std::wstring command_line = L"\"" + target_exe.wstring() + L"\"";
    if (lpCmdLine != nullptr && wcslen(lpCmdLine) > 0) {
        command_line += L" ";
        command_line += lpCmdLine;
    }

    // 5. Check if automated synchronous wait requested (e.g. --preflight-check or --wait)
    bool wait_for_completion = false;
    if (lpCmdLine != nullptr) {
        if (wcsstr(lpCmdLine, L"--preflight-check") != nullptr ||
            wcsstr(lpCmdLine, L"--verify-preflight") != nullptr ||
            wcsstr(lpCmdLine, L"--wait") != nullptr) {
            wait_for_completion = true;
        }
    }

    // 6. Launch workbench_desktop.exe with working directory explicitly set to package folder
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = static_cast<WORD>(nCmdShow > 0 ? nCmdShow : SW_SHOWNORMAL);

    PROCESS_INFORMATION pi{};
    std::wstring working_dir = launcher_dir.wstring();

    BOOL success = CreateProcessW(
        target_exe.c_str(),
        &command_line[0],
        nullptr, // Process handle not inheritable
        nullptr, // Thread handle not inheritable
        FALSE,   // Set handle inheritance to FALSE
        0,       // Standard creation flags (GUI process)
        nullptr, // Use parent's environment block
        working_dir.c_str(), // Set working directory to package directory
        &si,
        &pi
    );

    if (!success) {
        DWORD err_code = GetLastError();
        std::wstring err_msg = get_last_error_message(err_code);
        std::wstring alert = L"Failed to launch workbench_desktop.exe (Windows Error " +
                             std::to_wstring(err_code) + L"):\n\n" +
                             err_msg + L"\n\n" +
                             L"Command: " + command_line;
        show_error_dialog(alert);
        return 5;
    }

    // 7. Handle process lifecycle
    if (wait_for_completion) {
        // Wait up to 60 seconds for preflight verification
        WaitForSingleObject(pi.hProcess, 60000);
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return static_cast<int>(exit_code);
    }

    // Interactive launch: Detach and let desktop application run independently
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
