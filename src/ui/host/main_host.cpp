#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <memory>
#include "ui/host/webview_host.h"

namespace {

const wchar_t kWindowClassName[] = L"UnifiedEngineeringWorkbenchWindowClass";
const wchar_t kWindowTitle[] = L"Unified Engineering Workbench";
std::unique_ptr<workbench::ui::WebViewHost> g_host;

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_SIZE:
            if (g_host) {
                g_host->resize(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;

        case WM_GETMINMAXINFO: {
            LPMINMAXINFO mmi = reinterpret_cast<LPMINMAXINFO>(lParam);
            mmi->ptMinTrackSize.x = 360; // Allow testing mobile viewport down to 360px-375px
            mmi->ptMinTrackSize.y = 480;
            return 0;
        }

        case workbench::ui::WM_APP_BRIDGE_ASYNC_RESPONSE: {
            auto* resp_str = reinterpret_cast<std::string*>(lParam);
            if (resp_str) {
                if (g_host && g_host->is_initialized()) {
                    g_host->post_message_to_frontend(*resp_str);
                }
                delete resp_str;
            }
            return 0;
        }

        case WM_DESTROY:
            if (g_host) {
                g_host->close();
                g_host.reset();
            }
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR lpCmdLine, int nCmdShow) {
    // Check command line arguments for headless preflight verification
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);
    bool headless_preflight = false;
    if (argv != nullptr) {
        for (int i = 0; i < argc; ++i) {
            if (wcscmp(argv[i], L"--preflight-check") == 0 || wcscmp(argv[i], L"--verify-preflight") == 0) {
                headless_preflight = true;
            }
        }
        LocalFree(argv);
    }

    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return 1;
    }

    // Preflight Check for Microsoft Edge WebView2 Runtime
    auto runtime_res = workbench::ui::WebViewHost::check_webview_runtime();
    if (runtime_res.is_error()) {
        if (!headless_preflight) {
            MessageBoxW(
                nullptr,
                L"Microsoft Edge WebView2 Evergreen Runtime is required to run Unified Engineering Workbench.\n\n"
                L"Please download and install the WebView2 Runtime from:\n"
                L"https://go.microsoft.com/fwlink/p/?LinkId=2124703",
                L"Unified Engineering Workbench - Runtime Required",
                MB_ICONERROR | MB_OK
            );
        }
        CoUninitialize();
        return 2;
    }

    if (headless_preflight) {
        // Successful headless preflight verification
        CoUninitialize();
        return 0;
    }

    // Register Window Class
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&wc)) {
        CoUninitialize();
        return 3;
    }

    // Initial desktop window size: 1280x800, centered
    const int init_width = 1280;
    const int init_height = 800;
    const int screen_width = GetSystemMetrics(SM_CXSCREEN);
    const int screen_height = GetSystemMetrics(SM_CYSCREEN);
    const int pos_x = (screen_width - init_width) / 2;
    const int pos_y = (screen_height - init_height) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        pos_x > 0 ? pos_x : CW_USEDEFAULT,
        pos_y > 0 ? pos_y : CW_USEDEFAULT,
        init_width,
        init_height,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        CoUninitialize();
        return 4;
    }

    // Initialize Host and WebView2
    g_host = std::make_unique<workbench::ui::WebViewHost>();
    workbench::core::Status init_status = g_host->initialize(hwnd);
    if (init_status.is_error()) {
        MessageBoxW(
            hwnd,
            L"Failed to initialize WebView2 interface. The window will close.",
            L"Unified Engineering Workbench - Initialization Error",
            MB_ICONERROR | MB_OK
        );
        DestroyWindow(hwnd);
        CoUninitialize();
        return 5;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Standard message loop
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
