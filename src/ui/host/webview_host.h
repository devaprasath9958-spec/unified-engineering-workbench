#ifndef WORKBENCH_UI_HOST_WEBVIEW_HOST_H
#define WORKBENCH_UI_HOST_WEBVIEW_HOST_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wrl.h>
#include <string>
#include <memory>
#include "WebView2.h"
#include "core/result.h"
#include "ui/bridge/host_bridge.h"

namespace workbench::ui {

constexpr UINT WM_APP_BRIDGE_ASYNC_RESPONSE = WM_APP + 101;

/**
 * @brief Manages the lifecycle of the Microsoft Edge WebView2 control in a Win32 window.
 */
class WebViewHost {
public:
    WebViewHost();
    ~WebViewHost();

    // Disable copy/move
    WebViewHost(const WebViewHost&) = delete;
    WebViewHost& operator=(const WebViewHost&) = delete;

    /**
     * @brief Preflight check: verifies Microsoft Edge WebView2 Evergreen Runtime is installed.
     * @return Status with installed version string if successful, or error if missing.
     */
    static core::Result<std::wstring> check_webview_runtime();

    /**
     * @brief Resolves the local absolute file URI for index.html.
     */
    static std::wstring resolve_frontend_uri();

    /**
     * @brief Initialize WebView2 on the given parent window and navigate to the initial URI.
     */
    core::Status initialize(HWND parent_hwnd, const std::wstring& initial_uri = L"");

    /**
     * @brief Resize the WebView2 controller to match the window client rect.
     */
    void resize(int width, int height);

    /**
     * @brief Safely cleans up COM objects, unregisters event tokens, and closes the controller.
     */
    void close();

    /**
     * @brief Send a raw JSON string to the frontend via postMessage.
     */
    bool post_message_to_frontend(const std::string& json_message);

    /**
     * @brief Access the secure host bridge.
     */
    HostBridge& get_bridge() { return bridge_; }
    const HostBridge& get_bridge() const { return bridge_; }

    /**
     * @brief Check whether WebView2 has finished initializing and loading.
     */
    bool is_initialized() const { return initialized_; }

private:
    HWND parent_hwnd_{nullptr};
    bool initialized_{false};
    std::wstring initial_uri_;

    HostBridge bridge_;

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> webview_env_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> webview_controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview_view_;

    EventRegistrationToken web_message_token_{};
    EventRegistrationToken nav_completed_token_{};

    void setup_event_handlers();
};

} // namespace workbench::ui

#endif // WORKBENCH_UI_HOST_WEBVIEW_HOST_H
