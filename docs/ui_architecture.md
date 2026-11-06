# Desktop UI Architecture & Application Shell (Stage 9.1)

## 1. System Overview

The **Unified Engineering Workbench** desktop application combines a high-performance **native Win32 C++ host** with a minimal, restrained **Microsoft Edge WebView2 presentation layer**.

The architecture adheres strictly to engineering software design principles:
- **Zero Heavy Web Frameworks:** Pure HTML5, modular CSS with central design tokens, and lightweight Vanilla JavaScript.
- **Strict Separation of Concerns:** All numerical calculations, geometry parsing, thermal FEM solving, multicore benchmarking, and hardware adapter simulations remain 100% inside the C++ backend (`workbench_core`). The frontend is responsible exclusively for presentation, user interaction, and dashboard monitoring.
- **Offline-Ready Packaging:** The Microsoft Edge WebView2 C++ SDK (`WebView2.h`, `WebView2Loader.dll.lib`, and `WebView2Loader.dll`) is hosted locally in `third_party/webview2/`. The application and its frontend assets require zero internet connectivity once the Microsoft Edge WebView2 Evergreen Runtime is installed on the host operating system.

---

## 2. Directory & Component Structure

```text
UnifiedEngineeringWorkbench/
├── CMakeLists.txt                      # Central CMake build definition
├── third_party/
│   └── webview2/                       # Official Microsoft WebView2 SDK
│       ├── include/                    # WebView2.h & WebView2EnvironmentOptions.h
│       ├── lib/x64/                    # WebView2Loader.dll.lib
│       └── bin/x64/                    # WebView2Loader.dll (redistributable)
├── src/
│   ├── ui/
│   │   ├── host/                       # Native Win32 Desktop Host
│   │   │   ├── main_host.cpp           # Win32 entry point, window proc, message loop
│   │   │   ├── webview_host.h          # Host controller & COM lifecycle
│   │   │   └── webview_host.cpp        # WebView2 environment, controller & navigation
│   │   ├── bridge/                     # Secure C++ <-> Frontend Message Bridge
│   │   │   ├── host_bridge.h           # Safe IPC interface & status queries
│   │   │   └── host_bridge.cpp         # Origin validation, size bounds & action dispatch
│   │   └── frontend/                   # UI Presentation Layer (HTML / CSS / Vanilla JS)
│   │       ├── index.html              # Main application shell with 5-view navigation
│   │       ├── css/
│   │       │   ├── tokens.css          # Design tokens (Light/Dark mode, typography, spacing)
│   │       │   ├── base.css            # Reset, typography utilities, scrollbars
│   │       │   ├── components.css      # Shell layout, sidebar, header, cards, badges
│   │       │   └── responsive.css      # 375px (Mobile), 768px (Tablet), 1366px+ (Desktop)
│   │       └── js/
│   │           ├── icons.js            # Inline Lucide vector SVGs (zero external CDN)
│   │           ├── theme.js            # Persistent Light/Dark theme manager (localStorage)
│   │           ├── navigation.js       # View routing & mobile drawer toggle
│   │           └── app.js              # Application bootstrap & host handshake listener
└── tests/
    └── test_ui_host.cpp                # Headless unit tests for bridge, preflight & path resolver
```

---

## 3. Desktop Host & WebView2 Integration

### 3.1 Preflight Runtime Check
The native desktop host performs an explicit preflight verification via `GetAvailableCoreWebView2BrowserVersionString(nullptr, &versionString)`.
- **Runtime Detected:** The host records the detected runtime version (e.g., `153.0.4234.48`), initializes COM apartment threading, and proceeds with environment creation.
- **Runtime Missing:** The host cleanly intercepts the absence of the runtime, displays an actionable Win32 error dialog with the official download URL (`https://go.microsoft.com/fwlink/p/?LinkId=2124703`), and exits gracefully without crashing.
- **Headless Mode:** Passing `--preflight-check` executes the preflight test and exits immediately with code 0 if the runtime is present.

### 3.2 User Data Folder Isolation
WebView2 browser profiles and caches are stored in:
`%LOCALAPPDATA%\UnifiedEngineeringWorkbench\WebView2Cache`
(with fallback to `%TEMP%\UEW_WebView2Cache`). This prevents browser data from polluting the source tree or workspace directories.

### 3.3 Safe COM Lifecycle & Event Cleanup
All COM pointers are managed using `Microsoft::WRL::ComPtr`:
- `ICoreWebView2Environment`
- `ICoreWebView2Controller`
- `ICoreWebView2`

On window destruction (`WM_DESTROY`):
1. Event handler tokens (`web_message_token_`, `nav_completed_token_`) are explicitly deregistered via `remove_WebMessageReceived` and `remove_NavigationCompleted`.
2. `controller->Close()` is explicitly called to release native browser handles.
3. COM interface references are reset.
4. `CoUninitialize()` is invoked on application exit.

---

## 4. Secure Host Bridge

Communication between the JavaScript frontend and the native C++ desktop host is governed by the `HostBridge` class:

1. **Origin Verification:** Only requests originating from `file://`, `https://workbench.local`, or the local browser window are permitted. Arbitrary external origins (e.g. `https://attacker.com`) are rejected with `security_error`.
2. **Payload Size Bounds:** Incoming messages are limited to a strict upper bound of 64 KB (`65,536` bytes).
3. **Action Whitelist:** Only explicit, permitted actions are handled:
   - `ping`: Health check, responds with `pong`.
   - `handshake`: Initial connection synchronization, dynamically returns host platform, C++ standard, compiler info, runtime version, and loaded module counts.
   - `get_status`: Full subsystem registry report.
4. **Command & Path Security:** The bridge strictly rejects arbitrary shell commands, process executions, or unverified file paths.

---

## 5. Visual Design Direction & Theming

### 5.1 Color Tokens
The application enforces strict visual restraint. All neon accents, gradients, decorative animations, excessive shadows, and rounded cards are avoided.

| Token | Light Theme | Dark Theme | Purpose |
|---|---|---|---|
| `--bg-primary` | `#FFFFFF` | `#171717` | Main viewport & canvas background |
| `--bg-secondary` | `#F1F1F1` | `#292929` | Sidebar, top header, control background |
| `--text-primary` | `#171717` | `#FFFFFF` | Headings, titles, high-contrast labels |
| `--text-secondary` | `#737373` | `#A3A3A3` | Descriptions, captions, metadata |
| `--border-color` | `#E5E5E5` | `#383838` | Subtle component & panel dividing lines |

### 5.2 Dynamic Theme Switching
- The theme toggle button in the header instantly toggles between Light and Dark modes.
- The selection is stored in `localStorage` under `workbench-theme` and updates `document.documentElement.setAttribute('data-theme', theme)` in real time without requiring an application restart.
- Defaults to the Windows system preference (`prefers-color-scheme`) on initial launch.

### 5.3 Typography Foundations
The visual hierarchy is driven by standard Windows system typography:
- **Sans-serif Stack:** `"Segoe UI", -apple-system, BlinkMacSystemFont, Roboto, "Helvetica Neue", Arial, sans-serif`
- **Monospace Stack:** `"Cascadia Code", "Consolas", "Courier New", monospace`
- **Scale:**
  - H1 (Display Title): 22px / Weight 600 / Leading 1.25 / Tracking -0.015em
  - H2 (Section Title): 16px / Weight 600 / Leading 1.35
  - H3 (Card Title): 14px / Weight 600 / Leading 1.35
  - Body Text: 13px / Weight 400 / Leading 1.5
  - Captions & Small: 12px / Weight 400
  - Badges & Micro: 11px / Weight 500 / Tracking 0.035em

---

## 6. Responsive Layout Breakpoints

The layout is tested and validated across three distinct form factors:

1. **Mobile (< 640px, tested at 375px):**
   - Sidebar collapses into an off-canvas drawer (`transform: translateX(-100%)`).
   - Header reveals a hamburger menu button to open the navigation drawer over a dark backdrop.
   - Cards grid collapses to a single column (`grid-template-columns: 1fr`).
   - Touch targets for navigation items maintain >= 42px height for accessibility.
   - Zero horizontal overflow (`overflow-x: hidden`).

2. **Tablet (640px – 1023px, tested at 768px):**
   - Sidebar adapts to a compact 200px width.
   - Main content viewport padding scales to 16px (`var(--space-4)`).
   - Single-to-double column adaptive card grid.

3. **Desktop (>= 1024px, tested at 1366px and wider):**
   - Fixed 240px sidebar on the left.
   - Menu toggle button hidden.
   - Multi-column auto-fit card grid (`minmax(340px, 1fr)`).
   - Spacious 24px (`var(--space-6)`) content padding.

---

## 7. How to Build and Run

### 7.1 Build via CMake (MSVC x64 Release)
```powershell
# 1. Configure solution
cmake -B build -S . -A x64

# 2. Compile Release binaries
cmake --build build --config Release
```

### 7.2 Run Automated Tests
```powershell
# Run all 7 test suites via CTest (402 passing assertions)
ctest --test-dir build -C Release --output-on-failure

# Or run individual test executables:
.\build\Release\workbench_core_tests.exe
.\build\Release\workbench_geometry_tests.exe
.\build\Release\workbench_thermal_tests.exe
.\build\Release\workbench_performance_tests.exe
.\build\Release\workbench_hardware_tests.exe
.\build\Release\workbench_backprop_tests.exe
.\build\Release\workbench_ui_tests.exe
```

### 7.3 Launch Desktop Application
```powershell
# Launch the native desktop host application
.\build\Release\workbench_desktop.exe
```
Alternatively, double-click `workbench_desktop.exe` directly in Windows File Explorer. The post-build CMake step ensures `WebView2Loader.dll` and the `frontend/` directory are co-located next to the executable.

---

## 8. Current Limitations (Stage 9.1 Scope)
- **Navigation Shell Only:** The sidebar views for Geometry, Thermal Analysis, Performance, and Hardware present architecture status cards and parameter summaries; interactive CAD rendering (Three.js/WebGL), live thermal contour plots, and live hardware COM port communication will be connected in subsequent sub-stages (Stage 9.2+).
- **Runtime Dependency:** The standalone application binary requires that the Microsoft Edge WebView2 Evergreen Runtime is pre-installed on the host Windows system (inbox on Windows 11; standard on Windows 10).
