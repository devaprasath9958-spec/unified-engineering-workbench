# Third-Party Dependencies and Licensing

This document provides a comprehensive inventory of all third-party software components, libraries, runtimes, and development toolchains utilized by the **Unified Engineering Workbench**, along with their respective licensing terms and distribution scope.

---

## 1. Summary of Dependencies

| Component | Version | Vendor / Author | Scope | Licensing Terms |
| :--- | :---: | :--- | :--- | :--- |
| **Microsoft Edge WebView2 SDK** | `1.0.2903.40` | Microsoft Corporation | Redistributable Loader & C++ Headers | BSD-style (Headers); Microsoft Edge WebView2 SDK License (`WebView2Loader.dll`) |
| **Microsoft Visual C++ Standard Library** | MSVC `19.51.36260.0` | Microsoft Corporation | C++17 Standard Library Runtime | Microsoft Visual Studio EULA / Redistributable |
| **Microsoft Windows SDK** | `10.0.26100.0` | Microsoft Corporation | Win32 API Headers & Import Libraries | Microsoft Windows Software Development Kit EULA |

---

## 2. Detailed License Information

### 2.1 Microsoft Edge WebView2 SDK
- **Version:** `1.0.2903.40`
- **Location in Repository:** `third_party/webview2/`
  - Headers: `third_party/webview2/include/WebView2.h`, `WebView2EnvironmentOptions.h`
  - Import Library: `third_party/webview2/x64/WebView2Loader.dll.lib`
  - Dynamic Link Library: `third_party/webview2/x64/WebView2Loader.dll`
- **Purpose:** Provides the C++ COM interface and dynamic link loader stub enabling `workbench_desktop.exe` and `Launch.exe` to instantiate the Microsoft Edge WebView2 environment, create webview controllers, and establish two-way asynchronous JSON message passing via `ICoreWebView2WebMessageReceivedEventHandler` and `PostWebMessageAsJson`.
- **Runtime Clarification:** `WebView2Loader.dll` is an API client stub (~165 KB) that dynamically discovers and attaches to the Evergreen WebView2 Runtime installed on the host operating system. The Evergreen Runtime is preinstalled on Windows 11 and maintained through Windows Update.
- **Licensing Text:**
  - Headers are licensed under the BSD-style license:
    ```text
    Copyright (C) Microsoft Corporation. All rights reserved.
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright notice...
      * Redistributions in binary form must reproduce the above copyright notice...
    ```
  - The binary `WebView2Loader.dll` is redistributable under the Microsoft Edge WebView2 SDK Software License Terms.

### 2.2 Microsoft Visual C++ Toolchain & Standard Library
- **Compiler Version:** MSVC `19.51.36260.0` (Visual Studio 18 2026 BuildTools `Hostx64/x64/cl.exe`)
- **Language Standard:** ISO C++17 (`/std:c++17`)
- **Included Standard Headers:** `<thread>`, `<atomic>`, `<chrono>`, `<cmath>`, `<filesystem>`, `<vector>`, `<string>`, `<sstream>`, `<fstream>`, `<memory>`, `<algorithm>`, `<functional>`, `<numeric>`, `<stdexcept>`.
- **Licensing Terms:** Standard library headers and runtime binaries are provided under the Microsoft Software License Terms for Visual Studio / Microsoft C++ Build Tools.

### 2.3 Microsoft Windows SDK
- **Version:** `10.0.26100.0`
- **Target OS:** Windows 10 x64 (Build 1809+) / Windows 11 x64
- **Included Headers:** `<windows.h>`, `<shellapi.h>`, `<shlwapi.h>`, `<commdlg.h>`, `<winnt.h>`.
- **Linked System Libraries:** `user32.lib`, `shell32.lib`, `shlwapi.lib`, `comdlg32.lib`, `ntdll.lib`, `ole32.lib`, `oleaut32.lib`.
- **Licensing Terms:** Governed by the Microsoft Windows Software Development Kit License Agreement.

---

## 3. Zero External Frontend Dependencies Notice

The entire frontend layer of the **Unified Engineering Workbench** was built from first principles with strict air-gap compliance:

1. **Zero External JavaScript Frameworks or Libraries:**
   - **No Three.js:** The 3D viewport is powered by a custom WebGL vertex/fragment shader pipeline (`canvas_3d.js`) implementing trackball orbit rotation, orthographic/perspective projections, wireframe/solid rasterization, and an automated 2D HTML5 Canvas fallback.
   - **No Chart.js / D3.js:** All charting functions are powered by a custom high-DPI canvas rendering engine (`chart_engine.js`) supporting analytical curves, finite element nodal overlays, bar plots, dual-axis telemetry graphs, and logarithmic loss histories.
   - **No React / Vue / Angular / jQuery:** Application routing, theme management, modal dialogs, toast notifications, and DOM manipulation are written in pure native ES6+ JavaScript.
2. **Zero External CSS Frameworks:**
   - **No Tailwind CSS, Bootstrap, or Material UI:** Responsive layouts, modern typography, card components, data tables, accessibility focus outlines, and light/dark theme CSS custom properties are authored in modular native CSS (`tokens.css`, `base.css`, `components.css`, `responsive.css`).
3. **Zero Remote CDNs or Network Endpoints:**
   - The application makes zero remote network requests, does not load remote fonts (e.g., Google Fonts), does not load remote icons, and does not require active internet connectivity. All vector icons are rendered via an inline SVG helper library (`icons.js`).
