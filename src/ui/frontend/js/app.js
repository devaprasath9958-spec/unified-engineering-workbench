/**
 * UNIFIED ENGINEERING WORKBENCH - APPLICATION BOOTSTRAP & SECURE HOST BRIDGE
 * Dynamically communicates with the C++ native desktop host via IPC.
 * Provides Promise-based request_id tracking and coordinates all module screens.
 */

const AppBridge = {
    pending: new Map(),
    counter: 0,

    send(action, payload = {}) {
        return new Promise((resolve, reject) => {
            const reqId = `req-${Date.now()}-${++this.counter}`;

            // Safety timeout (30 seconds for long computations)
            const timeoutTimer = setTimeout(() => {
                if (this.pending.has(reqId)) {
                    this.pending.delete(reqId);
                    reject(new Error(`Host bridge request timeout for action '${action}' (req_id: ${reqId}).`));
                }
            }, 30000);

            this.pending.set(reqId, { resolve, reject, timeoutTimer });

            if (window.chrome && window.chrome.webview) {
                try {
                    window.chrome.webview.postMessage(JSON.stringify({
                        request_id: reqId,
                        action: action,
                        payload: payload
                    }));
                } catch (err) {
                    clearTimeout(timeoutTimer);
                    this.pending.delete(reqId);
                    reject(err);
                }
            } else {
                clearTimeout(timeoutTimer);
                this.pending.delete(reqId);
                // Running in standalone browser preview: return mock/demo structure
                console.warn(`[AppBridge] Standalone browser mode: mock response for '${action}'.`);
                resolve({ success: false, error: 'Host bridge is not connected (standalone browser).' });
            }
        });
    },

    handleMessage(data) {
        let msg = data;
        if (typeof data === 'string') {
            try {
                msg = JSON.parse(data);
            } catch (e) {
                console.error('[AppBridge] Failed to parse host message JSON:', e);
                return;
            }
        }

        if (!msg) return;

        // Check if there is a pending Promise waiting for this request_id
        if (msg.request_id && this.pending.has(msg.request_id)) {
            const entry = this.pending.get(msg.request_id);

            // If it's an indeterminate async started event, keep Promise pending
            if (msg.action === 'benchmark_started') {
                return;
            }

            clearTimeout(entry.timeoutTimer);
            this.pending.delete(msg.request_id);
            entry.resolve(msg);
            return;
        }

        // Global status or unsolicited notification
        if (msg.action === 'handshake_ack' || msg.action === 'status_report') {
            App.onHandshake(msg);
        }
    }
};

window.AppBridge = AppBridge;

const App = {
    hostConnected: false,
    systemInfo: null,

    init() {
        injectIcons();
        ThemeManager.init();
        NavigationManager.init();

        this.setupBridge();

        // Initialize Engineering Module Screens
        if (window.GeometryView) GeometryView.init();
        if (window.ThermalView) ThermalView.init();
        if (window.PerfView) PerfView.init();
        if (window.HardwareView) HardwareView.init();
        if (window.BackpropView) BackpropView.init();

        // Reveal body once initialization and style bindings are fully applied
        document.body.classList.add('ready');
    },

    setupBridge() {
        const dot = document.getElementById('host-status-dot');
        const text = document.getElementById('host-status-text');

        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.addEventListener('message', (event) => {
                AppBridge.handleMessage(event.data);
            });

            // Handshake with native C++ host
            AppBridge.send('handshake', {})
                .then(resp => {
                    if (resp && resp.success) {
                        this.onHandshake(resp);
                    }
                })
                .catch(err => {
                    console.warn('[App] Initial handshake error:', err);
                });
        } else {
            if (dot) dot.classList.remove('connected');
            if (text) text.textContent = 'Preview Mode (Browser)';
            this.updateSystemStatusUI({
                app_name: 'Unified Engineering Workbench',
                app_version: '1.0.0',
                host_platform: 'Browser Preview',
                cpp_standard: 'C++17 (Standby)',
                compiler_info: 'MSVC x64 (Native)',
                webview_runtime: 'Local Viewport',
                backend_ready: false,
                modules_count: 6
            });
        }
    },

    onHandshake(msg) {
        this.hostConnected = true;
        this.systemInfo = msg.payload;

        const dot = document.getElementById('host-status-dot');
        const text = document.getElementById('host-status-text');
        if (dot) dot.classList.add('connected');
        if (text) text.textContent = 'C++ Backend Connected';

        this.updateSystemStatusUI(this.systemInfo);

        // Refresh Geometry sample once live C++ host handshake completes
        if (window.GeometryView && !GeometryView.currentMesh) {
            GeometryView.loadSampleStl('bracket');
        }
    },

    updateSystemStatusUI(info) {
        if (!info) return;

        const updateElem = (id, val) => {
            const el = document.getElementById(id);
            if (el) el.textContent = val;
        };

        updateElem('stat-platform', info.host_platform || 'Windows 11 x64');
        updateElem('stat-cpp', info.cpp_standard || 'C++17');
        updateElem('stat-compiler', info.compiler_info || 'MSVC x64');
        updateElem('stat-webview', info.webview_runtime || 'Active');
        updateElem('stat-backend-ready', info.backend_ready ? 'Operational (Verified)' : 'Pending Host Connection');
        updateElem('stat-modules-count', info.modules_count ? `${info.modules_count} Active Modules` : '6 Modules');
    }
};

document.addEventListener('DOMContentLoaded', () => {
    App.init();
});
