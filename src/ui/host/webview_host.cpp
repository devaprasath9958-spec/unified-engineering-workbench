#include "ui/host/webview_host.h"
#include <commdlg.h>
#include <shlobj.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <thread>

using namespace Microsoft::WRL;

namespace workbench::ui {

namespace {

std::string wide_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), &str[0], size_needed, nullptr, nullptr);
    return str;
}

std::wstring utf8_to_wide(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wstr[0], size_needed);
    return wstr;
}

std::wstring get_user_data_folder() {
    wchar_t local_app_data[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, local_app_data))) {
        std::filesystem::path p(local_app_data);
        p /= L"UnifiedEngineeringWorkbench";
        p /= L"WebView2Cache";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p.wstring();
    }
    // Fallback to temp directory
    wchar_t temp_path[MAX_PATH];
    if (GetTempPathW(MAX_PATH, temp_path) > 0) {
        std::filesystem::path p(temp_path);
        p /= L"UEW_WebView2Cache";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p.wstring();
    }
    return L"";
}

} // namespace

WebViewHost::WebViewHost() = default;

WebViewHost::~WebViewHost() {
    close();
}

core::Result<std::wstring> WebViewHost::check_webview_runtime() {
    LPWSTR version_info = nullptr;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version_info);
    if (SUCCEEDED(hr) && version_info != nullptr) {
        std::wstring version(version_info);
        CoTaskMemFree(version_info);
        if (!version.empty()) {
            return core::Result<std::wstring>::success(version);
        }
    }
    if (version_info != nullptr) {
        CoTaskMemFree(version_info);
    }
    return core::Result<std::wstring>::failure(
        core::StatusCode::FileNotFound,
        "Microsoft Edge WebView2 Evergreen Runtime was not detected on this system. "
        "Please ensure the WebView2 Runtime is installed (https://go.microsoft.com/fwlink/p/?LinkId=2124703)."
    );
}

std::wstring WebViewHost::resolve_frontend_uri() {
    // 1. Check next to the executable in frontend/index.html
    wchar_t exe_path_buf[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exe_path_buf, MAX_PATH) > 0) {
        std::filesystem::path exe_dir = std::filesystem::path(exe_path_buf).parent_path();
        std::filesystem::path candidate1 = exe_dir / L"frontend" / L"index.html";
        if (std::filesystem::exists(candidate1)) {
            std::wstring p = candidate1.wstring();
            std::replace(p.begin(), p.end(), L'\\', L'/');
            return L"file:///" + p;
        }
    }

    // 2. Check source directory structure (relative to current working directory)
    std::vector<std::filesystem::path> search_paths = {
        std::filesystem::current_path() / L"src" / L"ui" / L"frontend" / L"index.html",
        std::filesystem::current_path() / L"frontend" / L"index.html",
        std::filesystem::current_path() / L".." / L"src" / L"ui" / L"frontend" / L"index.html"
    };

    for (const auto& sp : search_paths) {
        if (std::filesystem::exists(sp)) {
            std::wstring p = std::filesystem::canonical(sp).wstring();
            std::replace(p.begin(), p.end(), L'\\', L'/');
            return L"file:///" + p;
        }
    }

    // Default fallback
    return L"file:///index.html";
}

core::Status WebViewHost::initialize(HWND parent_hwnd, const std::wstring& initial_uri) {
    if (parent_hwnd == nullptr) {
        return core::Status::failure(core::StatusCode::InvalidInput, "Parent HWND cannot be null.");
    }
    parent_hwnd_ = parent_hwnd;
    initial_uri_ = initial_uri.empty() ? resolve_frontend_uri() : initial_uri;

    // Check runtime preflight
    auto runtime_res = check_webview_runtime();
    if (runtime_res.is_error()) {
        return core::Status::failure(runtime_res.error().code, runtime_res.error().message);
    }

    const std::string runtime_version_utf8 = wide_to_utf8(runtime_res.value());
    bridge_.set_webview_runtime_version(runtime_version_utf8);

    std::wstring user_data_folder = get_user_data_folder();

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        user_data_folder.empty() ? nullptr : user_data_folder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || env == nullptr) {
                    return result;
                }
                webview_env_ = env;

                // Create Controller
                webview_env_->CreateCoreWebView2Controller(
                    parent_hwnd_,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT c_result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(c_result) || controller == nullptr) {
                                return c_result;
                            }
                            webview_controller_ = controller;
                            webview_controller_->get_CoreWebView2(&webview_view_);

                            // Setup size
                            RECT bounds;
                            GetClientRect(parent_hwnd_, &bounds);
                            webview_controller_->put_Bounds(bounds);
                            webview_controller_->put_IsVisible(FALSE);

                            // Setup handlers and navigate
                            setup_event_handlers();
                            webview_view_->Navigate(initial_uri_.c_str());

                            return S_OK;
                        }
                    ).Get()
                );
                return S_OK;
            }
        ).Get()
    );

    if (FAILED(hr)) {
        return core::Status::failure(
            core::StatusCode::ComputationError,
            "Failed to initiate CreateCoreWebView2EnvironmentWithOptions (HRESULT: " + std::to_string(hr) + ")."
        );
    }

    return core::Status::ok();
}

void WebViewHost::setup_event_handlers() {
    if (!webview_view_) return;

    // 1. WebMessageReceived
    webview_view_->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2* /*sender*/, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                LPWSTR message_raw = nullptr;
                HRESULT hr = args->TryGetWebMessageAsString(&message_raw);
                if (SUCCEEDED(hr) && message_raw != nullptr) {
                    std::wstring wmsg(message_raw);
                    CoTaskMemFree(message_raw);

                    LPWSTR source_raw = nullptr;
                    std::string origin_utf8;
                    if (SUCCEEDED(args->get_Source(&source_raw)) && source_raw != nullptr) {
                        origin_utf8 = wide_to_utf8(source_raw);
                        CoTaskMemFree(source_raw);
                    }

                    const std::string msg_utf8 = wide_to_utf8(wmsg);
                    if (msg_utf8.find("\"benchmark_run\"") != std::string::npos && parent_hwnd_ != nullptr) {
                        // Extract request_id if present
                        std::string req_id;
                        size_t id_pos = msg_utf8.find("\"request_id\"");
                        if (id_pos != std::string::npos) {
                            size_t c_pos = msg_utf8.find(':', id_pos);
                            if (c_pos != std::string::npos) {
                                size_t q1 = msg_utf8.find('\"', c_pos);
                                if (q1 != std::string::npos) {
                                    size_t q2 = msg_utf8.find('\"', q1 + 1);
                                    if (q2 != std::string::npos) {
                                        req_id = msg_utf8.substr(q1 + 1, q2 - q1 - 1);
                                    }
                                }
                            }
                        }
                        std::string started_ack = "{\"request_id\":\"" + req_id + "\",\"success\":true,\"action\":\"benchmark_started\",\"payload\":{\"status\":\"running\"}}";
                        post_message_to_frontend(started_ack);

                        HWND hwnd = parent_hwnd_;
                        std::thread([this, hwnd, msg_utf8, origin_utf8]() {
                            const std::string response_json = bridge_.process_message(msg_utf8, origin_utf8);
                            if (hwnd) {
                                auto* pStr = new std::string(response_json);
                                if (!PostMessageW(hwnd, WM_APP_BRIDGE_ASYNC_RESPONSE, 0, reinterpret_cast<LPARAM>(pStr))) {
                                    delete pStr;
                                }
                            }
                        }).detach();
                    } else if ((msg_utf8.find("\"dialog_open_file\"") != std::string::npos ||
                               msg_utf8.find("\"dialog_save_file\"") != std::string::npos) &&
                              parent_hwnd_ != nullptr &&
                              msg_utf8.find("\"simulate_cancel\"") == std::string::npos &&
                              msg_utf8.find("\"file_path\"") == std::string::npos) {
                        bool is_open = (msg_utf8.find("\"dialog_open_file\"") != std::string::npos);

                        // Extract request_id
                        std::string req_id;
                        size_t id_pos = msg_utf8.find("\"request_id\"");
                        if (id_pos != std::string::npos) {
                            size_t c_pos = msg_utf8.find(':', id_pos);
                            if (c_pos != std::string::npos) {
                                size_t q1 = msg_utf8.find('\"', c_pos);
                                if (q1 != std::string::npos) {
                                    size_t q2 = msg_utf8.find('\"', q1 + 1);
                                    if (q2 != std::string::npos) {
                                        req_id = msg_utf8.substr(q1 + 1, q2 - q1 - 1);
                                    }
                                }
                            }
                        }

                        wchar_t szFile[MAX_PATH] = { 0 };
                        OPENFILENAMEW ofn{};
                        ofn.lStructSize = sizeof(ofn);
                        ofn.hwndOwner = parent_hwnd_;
                        ofn.lpstrFile = szFile;
                        ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);

                        if (is_open) {
                            ofn.lpstrFilter = L"Engineering Files (*.stl;*.csv)\0*.stl;*.csv\0STL Models (*.stl)\0*.stl\0CSV Datasets (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
                            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                        } else {
                            ofn.lpstrFilter = L"CSV Datasets (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
                            ofn.lpstrDefExt = L"csv";
                            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
                        }

                        BOOL ok = is_open ? GetOpenFileNameW(&ofn) : GetSaveFileNameW(&ofn);
                        std::string resp_json;
                        if (ok) {
                            std::string path_utf8 = wide_to_utf8(szFile);
                            resp_json = "{\"request_id\":\"" + req_id + "\",\"success\":true,\"action\":\"" +
                                        (is_open ? "dialog_open_file_result" : "dialog_save_file_result") +
                                        "\",\"payload\":{\"cancelled\":false,\"file_path\":\"" + path_utf8 + "\"}}";
                        } else {
                            resp_json = "{\"request_id\":\"" + req_id + "\",\"success\":true,\"action\":\"" +
                                        (is_open ? "dialog_open_file_result" : "dialog_save_file_result") +
                                        "\",\"payload\":{\"cancelled\":true}}";
                        }
                        post_message_to_frontend(resp_json);
                    } else {
                        const std::string response_json = bridge_.process_message(msg_utf8, origin_utf8);
                        post_message_to_frontend(response_json);
                    }
                }
                return S_OK;
            }
        ).Get(),
        &web_message_token_
    );

    // 2. NavigationCompleted
    webview_view_->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this](ICoreWebView2* /*sender*/, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                BOOL is_success = FALSE;
                args->get_IsSuccess(&is_success);
                if (is_success) {
                    initialized_ = true;
                    if (webview_controller_) {
                        webview_controller_->put_IsVisible(TRUE);
                    }
                    // Proactively dispatch handshake/status to frontend once navigation completes
                    const HostSystemStatus status = bridge_.get_status();
                    std::string handshake_json = bridge_.process_message("{\"action\":\"handshake\"}", "file://");
                    post_message_to_frontend(handshake_json);
                }
                return S_OK;
            }
        ).Get(),
        &nav_completed_token_
    );
}

void WebViewHost::resize(int width, int height) {
    if (webview_controller_) {
        RECT bounds{ 0, 0, width, height };
        webview_controller_->put_Bounds(bounds);
    }
}

bool WebViewHost::post_message_to_frontend(const std::string& json_message) {
    if (!webview_view_) return false;
    std::wstring wmsg = utf8_to_wide(json_message);
    HRESULT hr = webview_view_->PostWebMessageAsJson(wmsg.c_str());
    return SUCCEEDED(hr);
}

void WebViewHost::close() {
    if (webview_view_) {
        if (web_message_token_.value != 0) {
            webview_view_->remove_WebMessageReceived(web_message_token_);
            web_message_token_.value = 0;
        }
        if (nav_completed_token_.value != 0) {
            webview_view_->remove_NavigationCompleted(nav_completed_token_);
            nav_completed_token_.value = 0;
        }
    }
    if (webview_controller_) {
        webview_controller_->Close();
    }
    webview_view_.Reset();
    webview_controller_.Reset();
    webview_env_.Reset();
    initialized_ = false;
}

} // namespace workbench::ui
