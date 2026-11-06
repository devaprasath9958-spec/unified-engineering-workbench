# Unified Engineering Workbench

A modular, high-performance C++17 workstation developed for the **PASS Science Works R&D Internship 2026–2027** assignment.

The Unified Engineering Workbench integrates:
- **Geometry Processing:** ASCII STL parsing, bounds calculation, normal evaluation, and 3D WebGL / 2D Canvas viewport visualization with temperature field colormapping.
- **Thermal Analysis:** 1D steady-state thermal Finite Element Method (FEM) solver with boundary conditions ($T_0, T_L, q_0, q_L$) and dense exact analytical verification ($O(h^2)$ spatial convergence).
- **Multicore Benchmarking:** Work-stealing parallel batch solver using C++17 atomics, dynamic load balancing, $|T_{\text{parallel}} - T_{\text{serial}}| \le 10^{-8\ \circ}\text{C}$ correctness guarantees, and Win32 hardware metadata extraction.
- **Hardware Integration:** Simulated device drivers for STM32F407G-DISC1, Digilent Basys 3, and Digilent JTAG-HS2 with telemetry streams, fault injection, and recovery sequences.
- **Backpropagation:** Exact manual chain-rule neural network regression with central finite difference gradient verification ($\Delta \le 10^{-5}$) and simultaneous parameter updates over 100 training steps ($J \rightarrow < 10^{-12}$).
- **Desktop UI Shell:** Native Win32 desktop application hosting an offline, air-gapped HTML5/CSS/JavaScript UI via Microsoft Edge WebView2 Evergreen Runtime with dynamic light/dark theme synchronization and responsive layouts across mobile, tablet, and desktop viewports.
- **Standalone Packaging:** Zero-dependency standalone Windows GUI launcher (`Launch.exe`) enabling single double-click operation from an extracted directory.

---

## 1. Quick Start: Running the Standalone Application

### End-User Requirements
- **Operating System:** Windows 11 x64 (or Windows 10 x64 Build 1809 or later).
- **Runtime Component:** Microsoft Edge WebView2 Evergreen Runtime (preinstalled by default on Windows 11).
- **No Terminal or IDE Required:** You do **not** need PowerShell, Command Prompt, CMake, Visual Studio, VS Code, Python, Node.js, npm, or administrator privileges to run the application.

### Launching the Application
```text
Extract ZIP Archive
    ↓
Open UnifiedEngineeringWorkbench folder
    ↓
Double-click Launch.exe
    ↓
Unified Engineering Workbench opens
```

1. Extract `UnifiedEngineeringWorkbench-1.0.0-win64.zip` to any folder on your machine.
2. Open the extracted `UnifiedEngineeringWorkbench` folder.
3. Double-click `Launch.exe`.
4. The workbench will initialize immediately without spawning a console window.

---

## 2. Standalone Package Layout

The standalone package staged in `package/UnifiedEngineeringWorkbench/` contains only runtime-essential files:

```text
package/
└── UnifiedEngineeringWorkbench/
    ├── Launch.exe               # Native Win32 GUI launcher (no console window)
    ├── workbench_desktop.exe    # Native C++ desktop host with WebView2 integration
    ├── WebView2Loader.dll       # Microsoft Edge WebView2 client loader stub
    ├── frontend/                # Complete offline HTML5/CSS3/JavaScript UI
    │   ├── index.html           # Main application interface and DOM shell
    │   ├── css/
    │   │   ├── tokens.css       # Design tokens (typography, spacing, colors)
    │   │   ├── base.css         # Reset and global surface themes
    │   │   ├── components.css   # Cards, tables, badges, dialogs, buttons
    │   │   └── responsive.css   # Multi-viewport responsive media queries
    │   └── js/
    │       ├── app.js           # Host bridge handshake and initialization
    │       ├── navigation.js    # Accessible tab navigation and routing
    │       ├── theme.js         # Light/Dark mode state and canvas updates
    │       ├── icons.js         # Inline SVG vector icon library
    │       ├── components.js    # Shared UI toasts and dialog controllers
    │       └── modules/
    │           ├── canvas_3d.js     # WebGL 3D orbit and 2D canvas fallback
    │           ├── chart_engine.js  # High-DPI canvas charting engine
    │           ├── geometry_view.js # STL & CSV temperature point cloud
    │           ├── thermal_view.js  # FEM configuration and exact comparison
    │           ├── perf_view.js     # Multicore benchmark controls & graphs
    │           ├── hardware_view.js # Device discovery and telemetry streams
    │           └── backprop_view.js # Gradient verification & loss curve
    └── data/
        └── samples/             # Sample datasets for offline testing
            ├── valid_bracket.stl
            ├── valid_triangle.stl
            ├── degenerate_facet.stl
            ├── valid_nodal_temperature.csv
            └── thermal_results_N10.csv
```

### Excluded from Runtime Package
The package strictly excludes all non-runtime files:
- No static libraries (`*.lib`)
- No unit test executables (`*_tests.exe`, `workbench_smoke_test.exe`)
- No build directories or cache files (`build/`, `CMakeFiles/`, `CMakeCache.txt`)
- No IDE project files (`*.vcxproj`, `*.sln`, `.vs/`)
- No C++ source code files (`*.cpp`, `*.h`)
- No compiler or linker intermediates (`*.obj`, `*.pdb`, `*.ilk`)

---

## 3. Microsoft Edge WebView2 Runtime Prerequisite

### Important Runtime Clarification
- **`WebView2Loader.dll` is NOT the complete WebView2 runtime.** It is a lightweight client stub (~165 KB) provided by Microsoft that discovers and links to the Evergreen WebView2 Runtime installed on the host operating system.
- On **Windows 11**, the Evergreen WebView2 Runtime is preinstalled as a built-in system component.
- If running on a minimal or enterprise Windows installation where the runtime is not present, `Launch.exe` and `workbench_desktop.exe` will detect its absence and display an informative dialog directing the user to Microsoft's official installer:
  - **Download Link:** [Microsoft Edge WebView2 Evergreen Runtime](https://go.microsoft.com/fwlink/p/?LinkId=2124703)
- The application never silently installs or downloads software in the background.

---

## 4. Developer Build & Packaging Instructions

For developers modifying the C++ engineering algorithms or desktop host:

### Prerequisites
1. **Windows 11 x64**
2. **CMake** (version 3.20 or later)
3. **Visual Studio** (2019, 2022, or 2026) with the *Desktop development with C++* workload (tested and verified with MSVC 19.51.36260.0 / Visual Studio 18 2026 BuildTools, Windows SDK 10.0.26100.0)

### Step 1: Configure CMake
```powershell
cmake -B build -S . -A x64
```

### Step 2: Compile Release Build
```powershell
cmake --build build --config Release
```
This builds all core libraries, unit tests, `workbench_desktop.exe`, and `Launch.exe`.

### Step 3: Run Automated Test Suites (All 7 Suites)
```powershell
ctest --test-dir build -C Release --output-on-failure
```
Currently passes **100% of tests (7/7 suites, 638 passing assertions)**.

### Step 4: Generate Standalone Distribution Package
```powershell
cmake --build build --config Release --target package_workbench
```
This command stages all required runtime files into `package/UnifiedEngineeringWorkbench/` and generates the self-contained archive `package/UnifiedEngineeringWorkbench-1.0.0-win64.zip`.

---

## 5. Third-Party Dependencies & Licensing

| Dependency | Version | Purpose | Licensing Terms |
| :--- | :---: | :--- | :--- |
| **Microsoft.Web.WebView2 SDK** | 1.0.2903.40 | C++ headers and loader DLL (`WebView2Loader.dll`) for WebView2 host | Copyright (C) Microsoft Corporation.<br>Governed by BSD-style license for headers and Microsoft Edge WebView2 SDK License for redistributable `WebView2Loader.dll`. |
| **Standard C++17 Library** | MSVC 19.51 (19.51.36260.0) | Standard library implementations (`<thread>`, `<atomic>`, `<filesystem>`, `<chrono>`, `<cmath>`) | Microsoft Visual C++ Redistributable / Visual Studio EULA. |
| **Windows SDK** | 10.0.26100.0 | Win32 system APIs (`windows.h`, `shellapi.h`, `shlwapi.h`, `commdlg.h`) | Microsoft Windows Software Development Kit EULA. |

All user interface code, 3D WebGL / 2D Canvas engines, charting routines, and engineering algorithms were authored natively for this project with zero external npm packages, zero external CSS frameworks, and zero remote CDNs. For full licensing details, see [docs/licenses.md](docs/licenses.md).

---

## 6. Technical Documentation & Submission Reports

Comprehensive documentation for all engineering modules and submission requirements is located in `docs/`:
- [Submission Audit & Compliance Matrix](docs/submission_audit.md) — Detailed mapping of all assignment requirements to verification evidence.
- [Submission Manifest](docs/submission_manifest.md) — Full repository file inventory and descriptions.
- [AI Assistance Disclosure](docs/ai_use_disclosure.md) — Transparent disclosure of AI tool usage, workflow, prompts, modifications, and ownership.
- [Third-Party Licenses](docs/licenses.md) — Complete inventory and licensing terms for dependencies.
- [Final Technical Report Draft](docs/final_report.md) — 6-page comprehensive technical report covering all modules.
- [1D Steady-State Thermal FEM Solver](docs/thermal_fem.md) — Mathematical formulation, Thomas algorithm, and $O(h^2)$ midpoint convergence.
- [Multicore Benchmark & Scaling](docs/performance_benchmark.md) — Batch simulation methodology, speedup, and efficiency results.
- [Hardware Integration & Mock Drivers](docs/hardware_integration.md) — Protocols, registers, telemetry, and fault recovery.
- [Backpropagation & Numerical Gradients](docs/backpropagation.md) — Chain rule derivations, finite-difference verification, and loss history.
- [Desktop UI Architecture](docs/ui_architecture.md) — Win32 + WebView2 desktop host, security allowlists, and event loop.



