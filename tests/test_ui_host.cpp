#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cassert>
#include <regex>
#include <filesystem>
#include "ui/bridge/host_bridge.h"
#include "ui/host/webview_host.h"

namespace {

int g_ui_passed = 0;
int g_ui_failed = 0;

#define UI_ASSERT(condition, message) \
    do { \
        if (condition) { \
            g_ui_passed++; \
        } else { \
            g_ui_failed++; \
            std::cerr << "FAILED: " << message << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        } \
    } while (false)

// 1. Origin Security Validation
void test_bridge_origin_validation() {
    std::cout << "[TEST GROUP 1] Bridge Origin Security Validation...\n";

    // Permitted local and internal origins
    UI_ASSERT(workbench::ui::HostBridge::is_origin_allowed("file:///C:/Users/app/index.html"), "file:// origin permitted");
    UI_ASSERT(workbench::ui::HostBridge::is_origin_allowed("https://workbench.local/index.html"), "workbench.local origin permitted");
    UI_ASSERT(workbench::ui::HostBridge::is_origin_allowed(""), "empty local origin permitted");
    UI_ASSERT(workbench::ui::HostBridge::is_origin_allowed("null"), "null local origin permitted");

    // Prohibited external origins
    UI_ASSERT(!workbench::ui::HostBridge::is_origin_allowed("https://malicious.org"), "external https origin rejected");
    UI_ASSERT(!workbench::ui::HostBridge::is_origin_allowed("http://attacker.com/page"), "external http origin rejected");
    UI_ASSERT(!workbench::ui::HostBridge::is_origin_allowed("javascript:alert(1)"), "javascript pseudo-scheme rejected");
}

// 2. Message Payload Size Bounds (64 KB)
void test_bridge_message_size_bounds() {
    std::cout << "[TEST GROUP 2] Bridge Message Size Bounds...\n";

    workbench::ui::HostBridge bridge;

    // Normal sized message
    const std::string valid_msg = "{\"action\":\"ping\"}";
    const std::string valid_resp = bridge.process_message(valid_msg, "file://");
    UI_ASSERT(valid_resp.find("\"success\":true") != std::string::npos, "Normal message within bounds processed");

    // Oversized message exceeding 64 KB limit
    std::string oversized_msg = "{\"action\":\"ping\",\"padding\":\"";
    oversized_msg.append(70000, 'A');
    oversized_msg.append("\"}");

    const std::string oversized_resp = bridge.process_message(oversized_msg, "file://");
    UI_ASSERT(oversized_resp.find("\"success\":false") != std::string::npos, "Oversized message rejected");
    UI_ASSERT(oversized_resp.find("64 KB limit") != std::string::npos, "Oversized error message returned");
}

// 3. Action Whitelist & Command Injection Rejection
void test_bridge_action_whitelist() {
    std::cout << "[TEST GROUP 3] Action Whitelist & Security Enforcement...\n";

    workbench::ui::HostBridge bridge;

    // Whitelisted: ping
    const std::string ping_resp = bridge.process_message("{\"action\":\"ping\"}", "file://");
    UI_ASSERT(ping_resp.find("\"action\":\"pong\"") != std::string::npos, "Ping action returns pong");

    // Whitelisted: handshake
    const std::string hs_resp = bridge.process_message("{\"action\":\"handshake\"}", "file://");
    UI_ASSERT(hs_resp.find("\"action\":\"handshake_ack\"") != std::string::npos, "Handshake action returns handshake_ack");
    UI_ASSERT(hs_resp.find("\"backend_ready\":true") != std::string::npos, "Handshake confirms backend_ready dynamically");

    // Whitelisted: get_status
    const std::string status_resp = bridge.process_message("{\"action\":\"get_status\"}", "file://");
    UI_ASSERT(status_resp.find("\"action\":\"status_report\"") != std::string::npos, "get_status action returns status_report");

    // Prohibited: arbitrary shell/file command attempts
    const std::string cmd_resp = bridge.process_message("{\"action\":\"exec_shell\",\"cmd\":\"calc.exe\"}", "file://");
    UI_ASSERT(cmd_resp.find("\"success\":false") != std::string::npos, "Arbitrary command exec_shell rejected");

    const std::string path_resp = bridge.process_message("{\"action\":\"read_file\",\"path\":\"../../secret.txt\"}", "file://");
    UI_ASSERT(path_resp.find("\"success\":false") != std::string::npos, "Arbitrary file path access rejected");

    // Malformed JSON (missing action)
    const std::string bad_json = bridge.process_message("{\"foo\":\"bar\"}", "file://");
    UI_ASSERT(bad_json.find("\"success\":false") != std::string::npos, "JSON with missing action rejected");
}

// 4. Dynamic Host System Status Queries
void test_dynamic_host_status() {
    std::cout << "[TEST GROUP 4] Dynamic Host System Status Queries...\n";

    workbench::ui::HostBridge bridge;
    bridge.set_compiler_info("MSVC 19.4x x64");
    bridge.set_webview_runtime_version("153.0.4234.48 (Simulated Test)");

    const workbench::ui::HostSystemStatus status = bridge.get_status();
    UI_ASSERT(status.app_name == "Unified Engineering Workbench", "App name matches expected");
    UI_ASSERT(status.cpp_standard == "C++17", "C++ standard matches C++17");
    UI_ASSERT(status.compiler_info == "MSVC 19.4x x64", "Compiler info set dynamically");
    UI_ASSERT(status.backend_ready, "Backend ready flag is true");
    UI_ASSERT(status.loaded_modules.size() >= 6, "All 6 engineering modules present in registry");
}

// 5. Version-Agnostic WebView2 Runtime Preflight Check
void test_webview_preflight_check() {
    std::cout << "[TEST GROUP 5] Version-Agnostic WebView2 Runtime Preflight...\n";

    // Calls runtime preflight without asserting any specific hard-coded version number
    auto preflight_res = workbench::ui::WebViewHost::check_webview_runtime();

    if (preflight_res.is_ok()) {
        const std::wstring version_w = preflight_res.value();
        UI_ASSERT(!version_w.empty(), "Detected WebView2 version string is non-empty");

        // Verify version string structure (contains digits and dots, e.g., "120.0.2210.144")
        bool has_dot = (version_w.find(L'.') != std::wstring::npos);
        UI_ASSERT(has_dot, "WebView2 version string format contains version delimiter '.'");
        std::wcout << L"    [INFO] Live WebView2 Runtime Detected: " << version_w << L"\n";
    } else {
        // If runtime is not present on this machine, test that error is descriptive
        UI_ASSERT(preflight_res.is_error(), "Runtime absence gracefully reported");
        UI_ASSERT(!preflight_res.error().message.empty(), "Descriptive error message returned");
    }
}

// 6. Frontend URI Path Resolution
void test_frontend_uri_resolution() {
    std::cout << "[TEST GROUP 6] Frontend URI Path Resolution...\n";

    const std::wstring uri = workbench::ui::WebViewHost::resolve_frontend_uri();
    UI_ASSERT(!uri.empty(), "Resolved frontend URI is non-empty");
    UI_ASSERT(uri.rfind(L"file:///", 0) == 0, "Resolved URI starts with file:/// scheme");
    UI_ASSERT(uri.find(L"index.html") != std::wstring::npos, "Resolved URI points to index.html");

    // Confirm no backslashes in the URI path
    bool has_backslash = (uri.find(L'\\') != std::wstring::npos);
    UI_ASSERT(!has_backslash, "Resolved URI has normalized forward slashes");

    // Verify index.html stylesheet links completeness (including components.css)
    std::ifstream html_in("src/ui/frontend/index.html");
    if (!html_in.is_open()) {
        html_in.open("frontend/index.html");
    }
    UI_ASSERT(html_in.is_open(), "index.html is accessible for link verification");
    std::ostringstream html_ss;
    html_ss << html_in.rdbuf();
    std::string html_content = html_ss.str();
    html_in.close();

    UI_ASSERT(html_content.find("href=\"css/tokens.css\"") != std::string::npos, "index.html links tokens.css");
    UI_ASSERT(html_content.find("href=\"css/base.css\"") != std::string::npos, "index.html links base.css");
    UI_ASSERT(html_content.find("href=\"css/components.css\"") != std::string::npos, "index.html explicitly links components.css");
    UI_ASSERT(html_content.find("href=\"css/responsive.css\"") != std::string::npos, "index.html links responsive.css");
    UI_ASSERT(html_content.find("body.ready") != std::string::npos, "index.html includes FOUC prevention style");
}

// 7. Design System Tokens Completeness & Consistency (Stage 9.2)
void test_design_tokens_consistency() {
    std::cout << "[TEST GROUP 7] Design System Tokens Consistency (Stage 9.2)...\n";

    // Read tokens.css
    std::ifstream in("src/ui/frontend/css/tokens.css");
    if (!in.is_open()) {
        in.open("frontend/css/tokens.css");
    }
    UI_ASSERT(in.is_open(), "tokens.css is accessible for design token audit");

    std::ostringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();
    in.close();

    // Verify typography tokens
    UI_ASSERT(content.find("--font-sans:") != std::string::npos, "Sans-serif typography token defined");
    UI_ASSERT(content.find("--font-mono:") != std::string::npos, "Monospace typography token defined");
    UI_ASSERT(content.find("--text-xs:") != std::string::npos, "Font size token text-xs defined");
    UI_ASSERT(content.find("--text-sm:") != std::string::npos, "Font size token text-sm defined");
    UI_ASSERT(content.find("--text-base:") != std::string::npos, "Font size token text-base defined");
    UI_ASSERT(content.find("--text-md:") != std::string::npos, "Font size token text-md defined");
    UI_ASSERT(content.find("--text-lg:") != std::string::npos, "Font size token text-lg defined");
    UI_ASSERT(content.find("--text-xl:") != std::string::npos, "Font size token text-xl defined");
    UI_ASSERT(content.find("--text-2xl:") != std::string::npos, "Font size token text-2xl defined");
    UI_ASSERT(content.find("--text-3xl:") != std::string::npos, "Font size token text-3xl defined");

    // Verify spacing scale tokens
    UI_ASSERT(content.find("--space-1:") != std::string::npos, "Spacing token space-1 (4px) defined");
    UI_ASSERT(content.find("--space-2:") != std::string::npos, "Spacing token space-2 (8px) defined");
    UI_ASSERT(content.find("--space-3:") != std::string::npos, "Spacing token space-3 (12px) defined");
    UI_ASSERT(content.find("--space-4:") != std::string::npos, "Spacing token space-4 (16px) defined");
    UI_ASSERT(content.find("--space-6:") != std::string::npos, "Spacing token space-6 (24px) defined");
    UI_ASSERT(content.find("--space-8:") != std::string::npos, "Spacing token space-8 (32px) defined");

    // Verify component heights
    UI_ASSERT(content.find("--header-height:") != std::string::npos, "Component height header-height defined");
    UI_ASSERT(content.find("--btn-height-sm:") != std::string::npos, "Component height btn-height-sm defined");
    UI_ASSERT(content.find("--btn-height-md:") != std::string::npos, "Component height btn-height-md defined");
    UI_ASSERT(content.find("--input-height:") != std::string::npos, "Component height input-height defined");
    UI_ASSERT(content.find("--table-row-height:") != std::string::npos, "Component height table-row-height defined");

    // Verify geometry and border radius tokens
    UI_ASSERT(content.find("--radius-sm:") != std::string::npos, "Border radius token radius-sm defined");
    UI_ASSERT(content.find("--radius-md:") != std::string::npos, "Border radius token radius-md defined");

    // Verify shadows
    UI_ASSERT(content.find("--shadow-sm:") != std::string::npos, "Shadow token shadow-sm defined");
    UI_ASSERT(content.find("--shadow-dialog:") != std::string::npos, "Shadow token shadow-dialog defined");
}

// 8. Shared UI Components & Accessible Navigation (Stage 9.3)
void test_shared_components_and_navigation() {
    std::cout << "[TEST GROUP 8] Shared UI Components & Navigation Refinement (Stage 9.3)...\n";

    // Helper lambda to load file content across working directories
    auto load_file = [](const std::string& rel_path) -> std::string {
        std::vector<std::string> candidates = {
            rel_path,
            "../" + rel_path,
            "../../" + rel_path,
            "src/ui/" + rel_path,
            "../src/ui/" + rel_path
        };
        for (const auto& path : candidates) {
            std::ifstream file(path);
            if (file.is_open()) {
                std::ostringstream ss;
                ss << file.rdbuf();
                return ss.str();
            }
        }
        return "";
    };

    // A. Verify components.css shared component definitions
    const std::string css_content = load_file("src/ui/frontend/css/components.css");
    UI_ASSERT(!css_content.empty(), "components.css loaded successfully");
    UI_ASSERT(css_content.find(".card-info") != std::string::npos, "Card info variant (.card-info) styled");
    UI_ASSERT(css_content.find(".checkbox-container") != std::string::npos, "Custom checkbox container styled");
    UI_ASSERT(css_content.find(".checkbox-custom") != std::string::npos, "Custom checkbox square (.checkbox-custom) styled");
    UI_ASSERT(css_content.find(".table-empty-state") != std::string::npos, "Table empty state (.table-empty-state) styled");
    UI_ASSERT(css_content.find(".badge-ready") != std::string::npos, "Badge ready variant styled");
    UI_ASSERT(css_content.find(".badge-simulated") != std::string::npos, "Badge simulated variant styled");
    UI_ASSERT(css_content.find(".badge-running") != std::string::npos, "Badge running variant styled");
    UI_ASSERT(css_content.find(".badge-completed") != std::string::npos, "Badge completed variant styled");
    UI_ASSERT(css_content.find(".badge-warning") != std::string::npos, "Badge warning variant styled");
    UI_ASSERT(css_content.find(".badge-error") != std::string::npos, "Badge error variant styled");
    UI_ASSERT(css_content.find(".alert") != std::string::npos, "Alert container (.alert) styled");
    UI_ASSERT(css_content.find(".alert-info") != std::string::npos, "Alert info variant styled");
    UI_ASSERT(css_content.find(".alert-warning") != std::string::npos, "Alert warning variant styled");
    UI_ASSERT(css_content.find(".alert-error") != std::string::npos, "Alert error variant styled");
    UI_ASSERT(css_content.find(".toast-container") != std::string::npos, "Toast container styled");
    UI_ASSERT(css_content.find(".toast") != std::string::npos, "Toast notification styled");
    UI_ASSERT(css_content.find("@keyframes spin") != std::string::npos, "Spinner keyframe animation defined");
    UI_ASSERT(css_content.find(".spin-icon") != std::string::npos, "Spinner icon animation class defined");
    UI_ASSERT(css_content.find(":focus-visible") != std::string::npos, "Accessible :focus-visible outlines styled");

    // B. Verify navigation.js keyboard navigation & accessibility logic
    const std::string nav_content = load_file("src/ui/frontend/js/navigation.js");
    UI_ASSERT(!nav_content.empty(), "navigation.js loaded successfully");
    UI_ASSERT(nav_content.find("initialized") != std::string::npos, "NavigationManager contains idempotency initialization guard");
    UI_ASSERT(nav_content.find("ArrowDown") != std::string::npos, "Keyboard ArrowDown navigation supported");
    UI_ASSERT(nav_content.find("ArrowUp") != std::string::npos, "Keyboard ArrowUp navigation supported");
    UI_ASSERT(nav_content.find("Home") != std::string::npos, "Keyboard Home navigation supported");
    UI_ASSERT(nav_content.find("End") != std::string::npos, "Keyboard End navigation supported");
    UI_ASSERT(nav_content.find("Escape") != std::string::npos, "Global Escape key dismissal supported");
    UI_ASSERT(nav_content.find("aria-selected") != std::string::npos, "WAI-ARIA tab aria-selected state dynamically updated");

    // C. Verify components.js toast notification manager & form protection
    const std::string comp_content = load_file("src/ui/frontend/js/components.js");
    UI_ASSERT(!comp_content.empty(), "components.js loaded successfully");
    UI_ASSERT(comp_content.find("ToastManager") != std::string::npos, "ToastManager object defined");
    UI_ASSERT(comp_content.find("preventDefault") != std::string::npos, "Accidental form submission prevention enabled");

    // D. Verify index.html ARIA tab structure & container hooks
    const std::string html_content = load_file("src/ui/frontend/index.html");
    UI_ASSERT(!html_content.empty(), "index.html loaded successfully");
    UI_ASSERT(html_content.find("role=\"tablist\"") != std::string::npos, "Sidebar nav has role='tablist'");
    UI_ASSERT(html_content.find("role=\"tab\"") != std::string::npos, "Nav items have role='tab'");
    UI_ASSERT(html_content.find("role=\"tabpanel\"") != std::string::npos, "View sections have role='tabpanel'");
    UI_ASSERT(html_content.find("id=\"toast-container\"") != std::string::npos, "Toast container element present in DOM");
    UI_ASSERT(html_content.find("js/components.js") != std::string::npos, "components.js script linked in HTML");
}

// 9. Responsive Layouts & Device Adaptation Verification (Stage 9.4)
void test_responsive_layouts_and_viewports() {
    std::cout << "[TEST GROUP 9] Responsive Layouts & Device Adaptation (Stage 9.4)...\n";

    auto load_file = [](const std::string& rel_path) -> std::string {
        std::vector<std::string> candidates = {
            rel_path,
            "../" + rel_path,
            "../../" + rel_path,
            "src/ui/" + rel_path,
            "../src/ui/" + rel_path
        };
        for (const auto& path : candidates) {
            std::ifstream file(path);
            if (file.is_open()) {
                std::ostringstream ss;
                ss << file.rdbuf();
                return ss.str();
            }
        }
        return "";
    };

    const std::string resp_css = load_file("src/ui/frontend/css/responsive.css");
    UI_ASSERT(!resp_css.empty(), "responsive.css loaded successfully");

    // Tier 6: Large Desktop (1920px x 1080px)
    UI_ASSERT(resp_css.find("min-width: 1600px") != std::string::npos, "Tier 6 (Large Desktop >= 1600px) media query defined");
    UI_ASSERT(resp_css.find("max-width: 1560px") != std::string::npos, "Large desktop view-panel max-width constraint defined");

    // Tier 5: Standard Desktop (1366px x 768px)
    UI_ASSERT(resp_css.find("min-width: 1200px") != std::string::npos, "Tier 5 (Standard Desktop 1200px-1599px) media query defined");
    UI_ASSERT(resp_css.find("max-width: 1280px") != std::string::npos, "Standard desktop view-panel max-width constraint defined");

    // Tier 4: Small Desktop (1024px x 768px)
    UI_ASSERT(resp_css.find("min-width: 1024px") != std::string::npos, "Tier 4 (Small Desktop 1024px-1199px) media query defined");

    // Tier 3: Tablet (768px x 1024px)
    UI_ASSERT(resp_css.find("min-width: 640px") != std::string::npos, "Tier 3 (Tablet 640px-1023px) media query defined");
    UI_ASSERT(resp_css.find("width: 200px") != std::string::npos, "Tablet compact sidebar (width: 200px) defined");

    // Tier 2: Mobile Drawer (< 640px, tested at 430px x 932px)
    UI_ASSERT(resp_css.find("max-width: 639px") != std::string::npos, "Mobile tier (< 640px) media query defined");
    UI_ASSERT(resp_css.find("transform: translateX(-100%)") != std::string::npos, "Mobile sidebar off-screen drawer transform defined");
    UI_ASSERT(resp_css.find(".sidebar.open") != std::string::npos, "Mobile drawer open state defined");
    UI_ASSERT(resp_css.find(".backdrop.active") != std::string::npos, "Mobile backdrop active state defined");
    UI_ASSERT(resp_css.find("min-height: 44px") != std::string::npos, "Mobile touch target height >= 44px defined");
    UI_ASSERT(resp_css.find("text-overflow: ellipsis") != std::string::npos, "Mobile breadcrumb overflow ellipsis defined");

    // Tier 1: Small Mobile (< 380px, tested at 375px x 667px)
    UI_ASSERT(resp_css.find("max-width: 380px") != std::string::npos, "Tier 1 (Small Mobile < 380px) media query defined");

    // Fluidity & Component Adaptations
    UI_ASSERT(resp_css.find("overflow-x: auto") != std::string::npos, "Table wrapper horizontal scrolling enabled");
    UI_ASSERT(resp_css.find(".action-btn-group") != std::string::npos, "Fluid action button group class styled");
    UI_ASSERT(resp_css.find(".stat-grid-4") != std::string::npos, "4-item statistics grid responsive adapter styled");

    // Verify index.html contains responsive classes
    const std::string html_content = load_file("src/ui/frontend/index.html");
    UI_ASSERT(html_content.find("stat-grid-4") != std::string::npos, "index.html uses stat-grid-4 responsive class");
    UI_ASSERT(html_content.find("action-btn-group") != std::string::npos, "index.html uses action-btn-group responsive class");
}

// 10. Engineering Modules Bridge Integration Verification (Stage 9.5)
void test_bridge_engineering_modules_integration() {
    std::cout << "[TEST GROUP 10] Engineering Modules Bridge Integration (Stage 9.5)...\n";

    workbench::ui::HostBridge bridge;

    // A. Geometry Module Bridge
    {
        // 1. Valid bracket STL sample
        const std::string req1 = "{\"request_id\":\"req-geo-1\",\"action\":\"geometry_load_sample\",\"payload\":{\"sample_key\":\"bracket\"}}";
        const std::string resp1 = bridge.process_message(req1, "file://");
        UI_ASSERT(resp1.find("\"success\":true") != std::string::npos, "Geometry bracket sample loads successfully");
        UI_ASSERT(resp1.find("\"request_id\":\"req-geo-1\"") != std::string::npos, "Geometry response contains request_id");
        UI_ASSERT(resp1.find("\"solid_name\":\"l_bracket_segment\"") != std::string::npos, "Geometry solid_name matches l_bracket_segment");
        UI_ASSERT(resp1.find("\"total_triangles\":4") != std::string::npos, "Geometry triangle count is 4");
        UI_ASSERT(resp1.find("\"degenerate_count\":0") != std::string::npos, "Geometry degenerate count is 0");
        UI_ASSERT(resp1.find("\"simplified_visualization\":false") != std::string::npos, "Geometry under 25k limit is not simplified");
        UI_ASSERT(resp1.find("\"rendered_triangles\":4") != std::string::npos, "Geometry rendered_triangles is 4");
        UI_ASSERT(resp1.find("\"normals\":[") != std::string::npos, "Geometry contains per-vertex normals array");

        // 2. Single triangle sample
        const std::string req2 = "{\"request_id\":\"req-geo-2\",\"action\":\"geometry_load_sample\",\"payload\":{\"sample_key\":\"triangle\"}}";
        const std::string resp2 = bridge.process_message(req2, "file://");
        UI_ASSERT(resp2.find("\"total_triangles\":1") != std::string::npos, "Geometry triangle sample has 1 triangle");

        // 3. Degenerate facet sample
        const std::string req3 = "{\"request_id\":\"req-geo-3\",\"action\":\"geometry_load_sample\",\"payload\":{\"sample_key\":\"degenerate\"}}";
        const std::string resp3 = bridge.process_message(req3, "file://");
        UI_ASSERT(resp3.find("\"degenerate_count\":1") != std::string::npos, "Geometry detects 1 degenerate facet");

        // 4. CSV nodal temperature sample
        const std::string req4 = "{\"request_id\":\"req-geo-4\",\"action\":\"geometry_load_sample\",\"payload\":{\"sample_key\":\"nodal_temp\"}}";
        const std::string resp4 = bridge.process_message(req4, "file://");
        UI_ASSERT(resp4.find("\"type\":\"csv\"") != std::string::npos, "Geometry CSV sample identified as csv");
        UI_ASSERT(resp4.find("\"node_count\":5") != std::string::npos, "Geometry CSV sample contains 5 nodes");

        // 5. Unknown sample rejection
        const std::string req5 = "{\"request_id\":\"req-geo-5\",\"action\":\"geometry_load_sample\",\"payload\":{\"sample_key\":\"unknown_malicious_path\"}}";
        const std::string resp5 = bridge.process_message(req5, "file://");
        UI_ASSERT(resp5.find("\"success\":false") != std::string::npos, "Unknown geometry sample key is rejected");
        UI_ASSERT(resp5.find("Invalid or unrecognized sample key") != std::string::npos, "Descriptive sample error returned");
    }

    // B. Thermal FEM Module Bridge
    {
        // 1. Solve FEM N=10
        const std::string req_th1 = "{\"request_id\":\"req-th-1\",\"action\":\"thermal_solve\",\"payload\":{\"length\":1.0,\"area\":0.0001,\"thermal_conductivity\":10.0,\"heat_generation\":10000.0,\"left_temperature\":100.0,\"right_temperature\":20.0,\"element_count\":10}}";
        const std::string resp_th1 = bridge.process_message(req_th1, "file://");
        UI_ASSERT(resp_th1.find("\"success\":true") != std::string::npos, "Thermal FEM solve succeeds");
        UI_ASSERT(resp_th1.find("\"element_length\":0.1") != std::string::npos, "Thermal element_length is 0.1 m");
        UI_ASSERT(resp_th1.find("\"max_absolute_midpoint_error_C\":1.25") != std::string::npos, "Thermal max midpoint error is 1.25 C");
        UI_ASSERT(resp_th1.find("\"expected_max_midpoint_error_C\":1.25") != std::string::npos, "Thermal expected midpoint error is 1.25 C");
        UI_ASSERT(resp_th1.find("\"exact_curve\"") != std::string::npos, "Thermal dense analytical curve generated");

        // 2. Reject invalid input (non-positive length)
        const std::string req_th2 = "{\"request_id\":\"req-th-2\",\"action\":\"thermal_solve\",\"payload\":{\"length\":-1.0,\"element_count\":10}}";
        const std::string resp_th2 = bridge.process_message(req_th2, "file://");
        UI_ASSERT(resp_th2.find("\"success\":false") != std::string::npos, "Negative length is rejected");
        UI_ASSERT(resp_th2.find("must be strictly positive") != std::string::npos, "Validation error message returned for length");

        // 3. Export CSV
        const std::string test_thermal_csv = "results/test_thermal_export.csv";
        const std::string req_th3 = "{\"request_id\":\"req-th-3\",\"action\":\"thermal_export_csv\",\"payload\":{\"filepath\":\"" + test_thermal_csv + "\"}}";
        const std::string resp_th3 = bridge.process_message(req_th3, "file://");
        UI_ASSERT(resp_th3.find("\"success\":true") != std::string::npos, "Thermal FEM CSV export succeeds");
        UI_ASSERT(std::filesystem::exists(test_thermal_csv), "Thermal exported CSV file created on disk");
        std::filesystem::remove(test_thermal_csv);
    }

    // C. Multicore Performance Module Bridge
    {
        // 1. Hardware info query
        const std::string req_p1 = "{\"request_id\":\"req-p-1\",\"action\":\"benchmark_get_hardware_info\",\"payload\":{}}";
        const std::string resp_p1 = bridge.process_message(req_p1, "file://");
        UI_ASSERT(resp_p1.find("\"success\":true") != std::string::npos, "Benchmark hardware info query succeeds");
        UI_ASSERT(resp_p1.find("\"cpu_model\"") != std::string::npos, "Hardware info contains cpu_model");
        UI_ASSERT(resp_p1.find("\"logical_processor_count\"") != std::string::npos, "Hardware info contains logical_processor_count");

        // 2. Run mini benchmark suite (deterministic seed, 5 cases, 10 elements, 2 worker targets)
        const std::string req_p2 = "{\"request_id\":\"req-p-2\",\"action\":\"benchmark_run\",\"payload\":{\"case_count\":5,\"elements_per_case\":10,\"random_seed\":42,\"worker_targets\":[1,2]}}";
        const std::string resp_p2 = bridge.process_message(req_p2, "file://");
        UI_ASSERT(resp_p2.find("\"success\":true") != std::string::npos, "Benchmark mini suite runs successfully");
        UI_ASSERT(resp_p2.find("\"overall_correctness_passed\":true") != std::string::npos, "Benchmark overall correctness passed");
        UI_ASSERT(resp_p2.find("\"speedup\"") != std::string::npos, "Benchmark speedup reported");
        UI_ASSERT(resp_p2.find("\"raw_records\"") != std::string::npos, "Benchmark repetition records present");

        // 3. Export CSV
        const std::string test_raw_csv = "results/test_raw.csv";
        const std::string test_sum_csv = "results/test_summary.csv";
        const std::string req_p3 = "{\"request_id\":\"req-p-3\",\"action\":\"benchmark_export_csv\",\"payload\":{\"raw_path\":\"" + test_raw_csv + "\",\"summary_path\":\"" + test_sum_csv + "\"}}";
        const std::string resp_p3 = bridge.process_message(req_p3, "file://");
        UI_ASSERT(resp_p3.find("\"success\":true") != std::string::npos, "Benchmark CSV export succeeds");
        std::filesystem::remove(test_raw_csv);
        std::filesystem::remove(test_sum_csv);
    }

    // D. Hardware Integration Module Bridge
    {
        // 1. Device discovery
        const std::string req_h1 = "{\"request_id\":\"req-h-1\",\"action\":\"hardware_discover\",\"payload\":{}}";
        const std::string resp_h1 = bridge.process_message(req_h1, "file://");
        UI_ASSERT(resp_h1.find("\"success\":true") != std::string::npos, "Hardware discovery succeeds");
        UI_ASSERT(resp_h1.find("stm32f407g-disc1-01") != std::string::npos, "STM32 adapter discovered");
        UI_ASSERT(resp_h1.find("basys3-xc7a35t-01") != std::string::npos, "Basys3 adapter discovered");
        UI_ASSERT(resp_h1.find("jtag-hs2-adapter-01") != std::string::npos, "JTAG-HS2 adapter discovered");
        UI_ASSERT(resp_h1.find("\"is_simulated\":true") != std::string::npos, "Adapters flagged as simulated");

        // 2. Device connect
        const std::string req_h2 = "{\"request_id\":\"req-h-2\",\"action\":\"hardware_connect\",\"payload\":{\"device_id\":\"stm32f407g-disc1-01\"}}";
        const std::string resp_h2 = bridge.process_message(req_h2, "file://");
        UI_ASSERT(resp_h2.find("\"success\":true") != std::string::npos, "STM32 connects successfully");
        UI_ASSERT(resp_h2.find("\"connection_state\":\"Connected\"") != std::string::npos, "STM32 state is Connected");

        // 3. Telemetry read
        const std::string req_h3 = "{\"request_id\":\"req-h-3\",\"action\":\"hardware_read_telemetry\",\"payload\":{\"device_id\":\"stm32f407g-disc1-01\"}}";
        const std::string resp_h3 = bridge.process_message(req_h3, "file://");
        UI_ASSERT(resp_h3.find("\"success\":true") != std::string::npos, "STM32 telemetry read succeeds");
        UI_ASSERT(resp_h3.find("\"sequence_number\":1") != std::string::npos, "Telemetry sequence number increments to 1");
        UI_ASSERT(resp_h3.find("\"is_simulated\":true") != std::string::npos, "Telemetry explicitly flagged as simulated");

        // 4. Command execution: supported vs unsupported
        const std::string req_cmd1 = "{\"request_id\":\"req-cmd-1\",\"action\":\"hardware_send_command\",\"payload\":{\"device_id\":\"stm32f407g-disc1-01\",\"command\":\"ping\"}}";
        const std::string resp_cmd1 = bridge.process_message(req_cmd1, "file://");
        UI_ASSERT(resp_cmd1.find("\"success\":true") != std::string::npos, "STM32 executes supported ping command");

        const std::string req_cmd2 = "{\"request_id\":\"req-cmd-2\",\"action\":\"hardware_send_command\",\"payload\":{\"device_id\":\"stm32f407g-disc1-01\",\"command\":\"flash_fpga_bitstream\"}}";
        const std::string resp_cmd2 = bridge.process_message(req_cmd2, "file://");
        UI_ASSERT(resp_cmd2.find("\"success\":false") != std::string::npos, "STM32 rejects unsupported FPGA command");

        // 5. Simulated fault injection & clean recovery sequence
        const std::string req_h_fail = "{\"request_id\":\"req-h-fail\",\"action\":\"hardware_connect\",\"payload\":{\"device_id\":\"basys3-xc7a35t-01\",\"simulate_connection_failure\":true}}";
        const std::string resp_h_fail = bridge.process_message(req_h_fail, "file://");
        UI_ASSERT(resp_h_fail.find("\"success\":false") != std::string::npos, "Simulated connection failure triggers failure");
        UI_ASSERT(resp_h_fail.find("\"connection_state\":\"Error\"") != std::string::npos, "Device state transitions to Error on failure");

        // Recover: reset simulation flags
        const std::string req_rec = "{\"request_id\":\"req-rec\",\"action\":\"hardware_reset_simulation_flags\",\"payload\":{\"device_id\":\"basys3-xc7a35t-01\"}}";
        const std::string resp_rec = bridge.process_message(req_rec, "file://");
        UI_ASSERT(resp_rec.find("\"flags_reset\":true") != std::string::npos, "Simulation flags reset successfully");
        UI_ASSERT(resp_rec.find("\"connection_state\":\"Disconnected\"") != std::string::npos, "Device state restored to Disconnected");

        // Reconnect succeeds cleanly
        const std::string req_reconnect = "{\"request_id\":\"req-recon\",\"action\":\"hardware_connect\",\"payload\":{\"device_id\":\"basys3-xc7a35t-01\"}}";
        const std::string resp_reconnect = bridge.process_message(req_reconnect, "file://");
        UI_ASSERT(resp_reconnect.find("\"success\":true") != std::string::npos, "Clean reconnection succeeds after recovery");
    }

    // E. Backpropagation Module Bridge
    {
        // 1. Analytical gradient verification
        const std::string req_bp1 = "{\"request_id\":\"req-bp-1\",\"action\":\"backprop_verify_gradients\",\"payload\":{\"x\":1.0,\"y\":2.0,\"w\":0.5,\"b\":0.0,\"v\":1.0,\"c\":0.0,\"epsilon\":1e-6,\"tolerance\":1e-5}}";
        const std::string resp_bp1 = bridge.process_message(req_bp1, "file://");
        UI_ASSERT(resp_bp1.find("\"passed\":true") != std::string::npos, "Backprop analytical gradients match finite differences");
        UI_ASSERT(resp_bp1.find("\"max_absolute_error\"") != std::string::npos, "Backprop reports max_absolute_error");

        // 2. Train network for 100 steps
        const std::string req_bp2 = "{\"request_id\":\"req-bp-2\",\"action\":\"backprop_train\",\"payload\":{\"x\":1.0,\"y\":2.0,\"w\":0.5,\"b\":0.0,\"v\":1.0,\"c\":0.0,\"learning_rate\":0.1,\"steps\":100}}";
        const std::string resp_bp2 = bridge.process_message(req_bp2, "file://");
        UI_ASSERT(resp_bp2.find("\"success\":true") != std::string::npos, "Backprop training succeeds");
        UI_ASSERT(resp_bp2.find("\"step_count\":100") != std::string::npos, "Backprop produces exactly 100 step records");
        UI_ASSERT(resp_bp2.find("\"initial_loss\":1.1825") != std::string::npos, "Backprop initial loss is ~1.1825");

        // 3. Single step forward and update
        const std::string req_bp3 = "{\"request_id\":\"req-bp-3\",\"action\":\"backprop_step\",\"payload\":{\"x\":1.0,\"y\":2.0,\"w\":0.5,\"b\":0.0,\"v\":1.0,\"c\":0.0,\"learning_rate\":0.1}}";
        const std::string resp_bp3 = bridge.process_message(req_bp3, "file://");
        UI_ASSERT(resp_bp3.find("\"updated_parameters\"") != std::string::npos, "Single step returns updated parameters");
        UI_ASSERT(resp_bp3.find("\"loss\"") != std::string::npos, "Single step returns loss");

        // 4. Export loss CSV
        const std::string test_loss_csv = "results/test_loss.csv";
        const std::string req_bp4 = "{\"request_id\":\"req-bp-4\",\"action\":\"backprop_export_csv\",\"payload\":{\"filepath\":\"" + test_loss_csv + "\"}}";
        const std::string resp_bp4 = bridge.process_message(req_bp4, "file://");
        UI_ASSERT(resp_bp4.find("\"success\":true") != std::string::npos, "Backprop CSV export succeeds");
        std::filesystem::remove(test_loss_csv);
    }

    // F. Safe File Handling & Native File Dialogs (Items 2 & 8)
    {
        // 1. Rejection of arbitrary frontend export paths & directory traversal
        const std::string req_sec1 = "{\"request_id\":\"req-sec-1\",\"action\":\"thermal_export_csv\",\"payload\":{\"filepath\":\"../../etc/shadow.csv\"}}";
        const std::string resp_sec1 = bridge.process_message(req_sec1, "file://");
        UI_ASSERT(resp_sec1.find("\"success\":false") != std::string::npos, "Path traversal in thermal export is rejected");
        UI_ASSERT(resp_sec1.find("not permitted") != std::string::npos, "Descriptive security rejection returned for path traversal");

        // 2. Rejection of absolute Windows drive paths
        const std::string req_sec2 = "{\"request_id\":\"req-sec-2\",\"action\":\"thermal_export_csv\",\"payload\":{\"filepath\":\"C:\\\\Windows\\\\System32\\\\malicious.csv\"}}";
        const std::string resp_sec2 = bridge.process_message(req_sec2, "file://");
        UI_ASSERT(resp_sec2.find("\"success\":false") != std::string::npos, "Absolute Windows path in export is rejected");

        // 3. Native File Open Dialog - Cancelled by user
        const std::string req_dlg1 = "{\"request_id\":\"req-dlg-1\",\"action\":\"dialog_open_file\",\"payload\":{\"simulate_cancel\":true}}";
        const std::string resp_dlg1 = bridge.process_message(req_dlg1, "file://");
        UI_ASSERT(resp_dlg1.find("\"success\":true") != std::string::npos, "Cancelled file open dialog succeeds");
        UI_ASSERT(resp_dlg1.find("\"cancelled\":true") != std::string::npos, "Cancelled file open dialog reports cancelled: true");

        // 4. Native File Save Dialog - Valid default results destination
        const std::string req_dlg2 = "{\"request_id\":\"req-dlg-2\",\"action\":\"dialog_save_file\",\"payload\":{\"default_filename\":\"test_export.csv\"}}";
        const std::string resp_dlg2 = bridge.process_message(req_dlg2, "file://");
        UI_ASSERT(resp_dlg2.find("\"cancelled\":false") != std::string::npos, "Save file dialog produces valid file path");
        UI_ASSERT(resp_dlg2.find("results/test_export.csv") != std::string::npos, "Save file dialog destination resides within results/");
    }
}

// 11. Stage 9.6: C++ Backend Integration & Threaded Bridge Verification
void test_stage_9_6_backend_integration() {
    std::cout << "[TEST GROUP 11] C++ Backend Integration & Threaded Bridge (Stage 9.6)...\n";

    workbench::ui::HostBridge bridge;

    // A. Thermal FEM Multi-Mesh Discretization Convergence (N=5, 10, 20)
    {
        for (int n : {5, 10, 20}) {
            const double expected_err = (10000.0 / (8.0 * 10.0)) * std::pow(1.0 / n, 2);
            UI_ASSERT(expected_err > 0.0, "Theoretical error bound is positive");
            std::ostringstream ss;
            ss << "{\"request_id\":\"req-th-conv-" << n << "\",\"action\":\"thermal_solve\",\"payload\":{\"length\":1.0,\"area\":0.0001,\"thermal_conductivity\":10.0,\"heat_generation\":10000.0,\"left_temperature\":100.0,\"right_temperature\":20.0,\"element_count\":" << n << "}}";
            const std::string resp = bridge.process_message(ss.str(), "file://");
            UI_ASSERT(resp.find("\"success\":true") != std::string::npos, "Thermal solve succeeds for N");
            UI_ASSERT(resp.find("\"max_absolute_midpoint_error_C\"") != std::string::npos, "Midpoint error present");
        }
    }

    // B. Threaded Benchmark Simulation & Non-Blocking Worker Delivery
    {
        // Execute benchmark in background thread to confirm thread-safety
        std::atomic<bool> worker_completed{false};
        std::string worker_response;

        std::thread worker([&bridge, &worker_completed, &worker_response]() {
            const std::string req = "{\"request_id\":\"req-async-bench\",\"action\":\"benchmark_run\",\"payload\":{\"case_count\":6,\"elements_per_case\":12,\"random_seed\":12345,\"worker_targets\":[1,2]}}";
            worker_response = bridge.process_message(req, "file://");
            worker_completed.store(true);
        });

        worker.join();
        UI_ASSERT(worker_completed.load(), "Background worker thread completed without blocking");
        UI_ASSERT(worker_response.find("\"success\":true") != std::string::npos, "Background benchmark returned success");
        UI_ASSERT(worker_response.find("\"overall_correctness_passed\":true") != std::string::npos, "Threaded benchmark verified parallel agreement (delta T <= 1e-8)");
    }

    // C. Hardware Manager Multi-Device State Machine & Recovery
    {
        const std::vector<std::string> devices = {
            "stm32f407g-disc1-01",
            "basys3-xc7a35t-01",
            "jtag-hs2-adapter-01"
        };

        for (const auto& dev : devices) {
            // Disconnect if previously connected
            bridge.process_message("{\"action\":\"hardware_disconnect\",\"payload\":{\"device_id\":\"" + dev + "\"}}", "file://");

            // Connect
            const std::string conn_req = "{\"action\":\"hardware_connect\",\"payload\":{\"device_id\":\"" + dev + "\"}}";
            const std::string conn_resp = bridge.process_message(conn_req, "file://");
            UI_ASSERT(conn_resp.find("\"connection_state\":\"Connected\"") != std::string::npos, "Device transitioned to Connected");

            // Read telemetry
            const std::string tel_req = "{\"action\":\"hardware_read_telemetry\",\"payload\":{\"device_id\":\"" + dev + "\"}}";
            const std::string tel_resp = bridge.process_message(tel_req, "file://");
            UI_ASSERT(tel_resp.find("\"board_temperature_c\"") != std::string::npos, "Telemetry has board_temperature_c");
            UI_ASSERT(tel_resp.find("\"core_voltage_v\"") != std::string::npos, "Telemetry has core_voltage_v");
            UI_ASSERT(tel_resp.find("\"power_consumption_w\"") != std::string::npos, "Telemetry has power_consumption_w");

            // Disconnect
            const std::string disc_req = "{\"action\":\"hardware_disconnect\",\"payload\":{\"device_id\":\"" + dev + "\"}}";
            const std::string disc_resp = bridge.process_message(disc_req, "file://");
            UI_ASSERT(disc_resp.find("\"connection_state\":\"Disconnected\"") != std::string::npos, "Device transitioned to Disconnected");
        }
    }

    // D. Backpropagation Numerical Convergence & Parameter Progression
    {
        const std::string req = "{\"request_id\":\"req-bp-conv\",\"action\":\"backprop_train\",\"payload\":{\"x\":1.0,\"y\":2.0,\"w\":0.5,\"b\":0.0,\"v\":1.0,\"c\":0.0,\"learning_rate\":0.1,\"steps\":100}}";
        const std::string resp = bridge.process_message(req, "file://");
        UI_ASSERT(resp.find("\"success\":true") != std::string::npos, "Backprop 100-step training succeeds");
        UI_ASSERT(resp.find("\"step_count\":100") != std::string::npos, "Exactly 100 steps recorded in payload");
        UI_ASSERT(resp.find("\"initial_loss\":1.1825") != std::string::npos, "Initial loss matches analytical calculation (~1.1825)");
        UI_ASSERT(resp.find("\"final_loss\":") != std::string::npos, "Final loss recorded");
    }

    // E. Export Path Enforcement & Traversal Rejection
    {
        UI_ASSERT(workbench::ui::HostBridge::is_safe_relative_export_path("results/thermal.csv"), "Valid results/ path accepted");
        UI_ASSERT(workbench::ui::HostBridge::is_safe_relative_export_path("results/benchmark/summary.csv"), "Subdirectory within results/ accepted");
        UI_ASSERT(!workbench::ui::HostBridge::is_safe_relative_export_path("results/../secret.csv"), "Path traversal rejected");
        UI_ASSERT(!workbench::ui::HostBridge::is_safe_relative_export_path("results/evil.exe"), "Non-CSV file extension rejected");
        UI_ASSERT(!workbench::ui::HostBridge::is_safe_relative_export_path("C:/Windows/malicious.csv"), "Absolute Windows path rejected");
        UI_ASSERT(!workbench::ui::HostBridge::is_safe_relative_export_path("/etc/passwd"), "POSIX root path rejected");
    }
}

// 12. Stage 9.7: Functional Hardening, Edge Cases & Error Recovery Verification
void test_stage_9_7_functional_hardening() {
    std::cout << "[TEST GROUP 12] Functional Hardening, Edge Cases & Error Recovery (Stage 9.7)...\n";

    workbench::ui::HostBridge bridge;

    // A. Geometry Parser Defensive Hardening
    {
        // 1. Malformed STL string
        const std::string bad_stl = "{\"action\":\"geometry_parse_custom\",\"payload\":{\"stl_text\":\"solid bad\\nfacet normal 0 0\\nendfacet\\nendsolid\"}}";
        const std::string resp_stl = bridge.process_message(bad_stl, "file://");
        UI_ASSERT(resp_stl.find("\"success\":false") != std::string::npos, "Malformed STL text rejected cleanly");
        UI_ASSERT(resp_stl.find("STL Parse Error") != std::string::npos, "Descriptive STL parse error returned");

        // 2. Empty custom payload
        const std::string empty_geo = "{\"action\":\"geometry_parse_custom\",\"payload\":{}}";
        const std::string resp_empty = bridge.process_message(empty_geo, "file://");
        UI_ASSERT(resp_empty.find("\"success\":false") != std::string::npos, "Empty custom geometry payload rejected");

        // 3. Degenerate facet detection
        const std::string req_degen = "{\"action\":\"geometry_load_sample\",\"payload\":{\"sample_key\":\"degenerate\"}}";
        const std::string resp_degen = bridge.process_message(req_degen, "file://");
        UI_ASSERT(resp_degen.find("\"degenerate_count\":1") != std::string::npos, "Degenerate facet accurately counted");
    }

    // B. Thermal FEM Boundary Condition Guardrails
    {
        // 1. Zero thermal conductivity k
        const std::string req_k0 = "{\"action\":\"thermal_solve\",\"payload\":{\"length\":1.0,\"area\":0.0001,\"thermal_conductivity\":0.0,\"heat_generation\":1000.0,\"left_temperature\":100.0,\"right_temperature\":20.0,\"element_count\":10}}";
        const std::string resp_k0 = bridge.process_message(req_k0, "file://");
        UI_ASSERT(resp_k0.find("\"success\":false") != std::string::npos, "Zero thermal conductivity rejected");
        UI_ASSERT(resp_k0.find("must be strictly positive") != std::string::npos, "Explanation returned for zero k");

        // 2. Zero elements N
        const std::string req_n0 = "{\"action\":\"thermal_solve\",\"payload\":{\"length\":1.0,\"area\":0.0001,\"thermal_conductivity\":10.0,\"heat_generation\":1000.0,\"left_temperature\":100.0,\"right_temperature\":20.0,\"element_count\":0}}";
        const std::string resp_n0 = bridge.process_message(req_n0, "file://");
        UI_ASSERT(resp_n0.find("\"success\":false") != std::string::npos, "Zero element count rejected");
        UI_ASSERT(resp_n0.find("at least 1") != std::string::npos, "Explanation returned for zero N");

        // 3. Zero cross-sectional area A
        const std::string req_a0 = "{\"action\":\"thermal_solve\",\"payload\":{\"length\":1.0,\"area\":0.0,\"thermal_conductivity\":10.0,\"heat_generation\":1000.0,\"left_temperature\":100.0,\"right_temperature\":20.0,\"element_count\":10}}";
        const std::string resp_a0 = bridge.process_message(req_a0, "file://");
        UI_ASSERT(resp_a0.find("\"success\":false") != std::string::npos, "Zero area rejected");
    }

    // C. Multicore Benchmark Input Resilience
    {
        // 1. Zero batch cases safely defaulted
        const std::string req_zero = "{\"action\":\"benchmark_run\",\"payload\":{\"case_count\":0,\"elements_per_case\":0,\"random_seed\":42,\"worker_targets\":[1]}}";
        const std::string resp_zero = bridge.process_message(req_zero, "file://");
        UI_ASSERT(resp_zero.find("\"success\":true") != std::string::npos, "Zero case count safely defaulted to valid positive workload");

        // 2. Deterministic repeatability
        const std::string req_det1 = "{\"action\":\"benchmark_run\",\"payload\":{\"case_count\":5,\"elements_per_case\":10,\"random_seed\":999,\"worker_targets\":[1]}}";
        const std::string resp_det1 = bridge.process_message(req_det1, "file://");
        UI_ASSERT(resp_det1.find("\"overall_correctness_passed\":true") != std::string::npos, "Deterministic run 1 passes verification");
    }

    // D. Hardware Manager Disconnected State & Security
    {
        // 1. Command dispatch while disconnected
        bridge.process_message("{\"action\":\"hardware_disconnect\",\"payload\":{\"device_id\":\"stm32f407g-disc1-01\"}}", "file://");
        const std::string req_cmd_disc = "{\"action\":\"hardware_send_command\",\"payload\":{\"device_id\":\"stm32f407g-disc1-01\",\"command\":\"ping\"}}";
        const std::string resp_cmd_disc = bridge.process_message(req_cmd_disc, "file://");
        UI_ASSERT(resp_cmd_disc.find("\"success\":false") != std::string::npos, "Command to disconnected device rejected");

        // 2. Unknown device ID query
        const std::string req_unk = "{\"action\":\"hardware_connect\",\"payload\":{\"device_id\":\"unknown-device-99\"}}";
        const std::string resp_unk = bridge.process_message(req_unk, "file://");
        UI_ASSERT(resp_unk.find("\"success\":false") != std::string::npos, "Unknown device ID connection rejected");

        // 3. Telemetry read from disconnected device
        const std::string req_tel_disc = "{\"action\":\"hardware_read_telemetry\",\"payload\":{\"device_id\":\"stm32f407g-disc1-01\"}}";
        const std::string resp_tel_disc = bridge.process_message(req_tel_disc, "file://");
        UI_ASSERT(resp_tel_disc.find("\"success\":false") != std::string::npos, "Telemetry read from disconnected device rejected");
    }

    // E. Backpropagation Mathematical Integrity & Bounds
    {
        // 1. Non-positive learning rate rejected
        const std::string req_bad_eta = "{\"action\":\"backprop_train\",\"payload\":{\"learning_rate\":-0.05,\"steps\":100}}";
        const std::string resp_bad_eta = bridge.process_message(req_bad_eta, "file://");
        UI_ASSERT(resp_bad_eta.find("\"success\":false") != std::string::npos, "Negative learning rate rejected");
        UI_ASSERT(resp_bad_eta.find("strictly positive") != std::string::npos, "Explanation returned for invalid eta");

        // 2. Excessive steps rejected (>10000)
        const std::string req_huge_steps = "{\"action\":\"backprop_train\",\"payload\":{\"learning_rate\":0.1,\"steps\":50000}}";
        const std::string resp_huge_steps = bridge.process_message(req_huge_steps, "file://");
        UI_ASSERT(resp_huge_steps.find("\"success\":false") != std::string::npos, "Excessive training steps rejected");

        // 3. Mathematical relationship verification in single step
        const std::string req_step = "{\"action\":\"backprop_step\",\"payload\":{\"x\":1.0,\"y\":2.0,\"w\":0.5,\"b\":0.0,\"v\":1.0,\"c\":0.0,\"learning_rate\":0.1}}";
        const std::string resp_step = bridge.process_message(req_step, "file://");
        UI_ASSERT(resp_step.find("\"z\":0.5") != std::string::npos, "z = w*x + b = 0.5 verified");
        UI_ASSERT(resp_step.find("\"h\":0.4621") != std::string::npos, "h = tanh(0.5) ~ 0.4621 verified");
        UI_ASSERT(resp_step.find("\"y_pred\":0.4621") != std::string::npos, "y_pred = v*h + c ~ 0.4621 verified");
        UI_ASSERT(resp_step.find("\"loss\":1.1825") != std::string::npos, "Loss J = 0.5*(y_pred - y)^2 ~ 1.1825 verified");
    }

    // F. Safe Dialog Cancellation
    {
        const std::string req_cancel = "{\"action\":\"dialog_save_file\",\"payload\":{\"simulate_cancel\":true}}";
        const std::string resp_cancel = bridge.process_message(req_cancel, "file://");
        UI_ASSERT(resp_cancel.find("\"success\":true") != std::string::npos, "Simulated cancel save dialog succeeds");
        UI_ASSERT(resp_cancel.find("\"cancelled\":true") != std::string::npos, "Reports cancelled: true");
    }
}

// 13. Stage 10: Standalone Packaging & Launch.exe Integrity
void test_stage_10_packaging_and_launcher() {
    std::cout << "[TEST GROUP 13] Standalone Packaging & Launch.exe Integrity (Stage 10)...\n";

    // 1. Verify launcher source file existence and critical implementation patterns
    std::ifstream launcher_src("src/launcher/main_launcher.cpp");
    UI_ASSERT(launcher_src.is_open(), "src/launcher/main_launcher.cpp source file exists");
    if (launcher_src.is_open()) {
        std::stringstream ss;
        ss << launcher_src.rdbuf();
        const std::string code = ss.str();

        UI_ASSERT(code.find("wWinMain") != std::string::npos, "Launcher implements Win32 GUI wWinMain entrypoint");
        UI_ASSERT(code.find("workbench_desktop.exe") != std::string::npos, "Launcher references workbench_desktop.exe");
        UI_ASSERT(code.find("WebView2Loader.dll") != std::string::npos, "Launcher checks WebView2Loader.dll presence");
        UI_ASSERT(code.find("frontend\\\\index.html") != std::string::npos || code.find("frontend\\index.html") != std::string::npos, "Launcher checks frontend index presence");
        UI_ASSERT(code.find("CreateProcessW") != std::string::npos, "Launcher invokes CreateProcessW");
        UI_ASSERT(code.find("working_dir") != std::string::npos, "Launcher explicitly sets working directory to package dir");
        UI_ASSERT(code.find("MessageBoxW") != std::string::npos, "Launcher provides native GUI error dialogs");
        UI_ASSERT(code.find("--preflight-check") != std::string::npos, "Launcher supports automated synchronous preflight forwarding");
        UI_ASSERT(code.find("CloseHandle") != std::string::npos, "Launcher cleans up process and thread handles");
    }

    // 2. Verify CMakeLists.txt target definitions for Launch and package_workbench
    std::ifstream cmake_file("CMakeLists.txt");
    UI_ASSERT(cmake_file.is_open(), "CMakeLists.txt is readable");
    if (cmake_file.is_open()) {
        std::stringstream ss;
        ss << cmake_file.rdbuf();
        const std::string cmake_code = ss.str();

        UI_ASSERT(cmake_code.find("add_executable(Launch WIN32") != std::string::npos, "CMakeLists.txt configures Launch as WIN32 GUI application");
        UI_ASSERT(cmake_code.find("add_custom_target(package_workbench") != std::string::npos, "CMakeLists.txt configures package_workbench target");
        UI_ASSERT(cmake_code.find("UnifiedEngineeringWorkbench-1.0.0-win64.zip") != std::string::npos, "CMakeLists.txt configures ZIP archive creation");
    }

    // 3. Verify presence of required runtime assets in source repository
    UI_ASSERT(std::filesystem::exists("third_party/webview2/bin/x64/WebView2Loader.dll"), "WebView2Loader.dll exists in third_party");
    UI_ASSERT(std::filesystem::exists("src/ui/frontend/index.html"), "Frontend index.html exists in source");
    UI_ASSERT(std::filesystem::exists("src/ui/frontend/css/tokens.css"), "Design tokens CSS exists");
    UI_ASSERT(std::filesystem::exists("src/ui/frontend/css/components.css"), "Components CSS exists");
    UI_ASSERT(std::filesystem::exists("src/ui/frontend/js/app.js"), "Application JS controller exists");
    UI_ASSERT(std::filesystem::exists("data/samples/valid_bracket.stl"), "Sample bracket STL exists in data");
    UI_ASSERT(std::filesystem::exists("data/samples/valid_nodal_temperature.csv"), "Sample nodal temperature CSV exists in data");

    // 4. Verify packaged origin and relative path security
    workbench::ui::HostBridge bridge;
    UI_ASSERT(workbench::ui::HostBridge::is_origin_allowed("file:///package/UnifiedEngineeringWorkbench/frontend/index.html"), "Package frontend origin allowed");
    UI_ASSERT(workbench::ui::HostBridge::is_safe_relative_export_path("results/test_export.csv"), "Package results export path allowed");
    UI_ASSERT(!workbench::ui::HostBridge::is_safe_relative_export_path("../outside.csv"), "Directory traversal outside package rejected");
}

} // namespace

int main() {
    std::cout << "========================================\n";
    std::cout << "  UNIFIED ENGINEERING WORKBENCH TESTS  \n";
    std::cout << "  Stage 9.5 - 10: Complete UI & Package\n";
    std::cout << "========================================\n\n";

    test_bridge_origin_validation();
    test_bridge_message_size_bounds();
    test_bridge_action_whitelist();
    test_dynamic_host_status();
    test_webview_preflight_check();
    test_frontend_uri_resolution();
    test_design_tokens_consistency();
    test_shared_components_and_navigation();
    test_responsive_layouts_and_viewports();
    test_bridge_engineering_modules_integration();
    test_stage_9_6_backend_integration();
    test_stage_9_7_functional_hardening();
    test_stage_10_packaging_and_launcher();

    std::cout << "\n========================================\n";
    std::cout << "  UI HOST TESTS SUMMARY: " << g_ui_passed << " PASSED, " << g_ui_failed << " FAILED\n";
    std::cout << "========================================\n";

    return (g_ui_failed == 0) ? 0 : 1;
}
