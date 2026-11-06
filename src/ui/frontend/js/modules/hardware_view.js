/**
 * UNIFIED ENGINEERING WORKBENCH - HARDWARE MODULE CONTROLLER
 * Connects UI to simulated hardware adapters via HostBridge.
 * Strictly adheres to supported APIs and clearly labels all simulated telemetry.
 */

const HardwareView = {
    chart: null,
    telemetryHistory: [],
    pollTimer: null,
    activeDeviceId: 'stm32f407g-disc1-01',

    init() {
        const canvas = document.getElementById('hardware-telemetry-canvas');
        if (canvas) {
            this.chart = new ChartEngine(canvas, {
                type: 'line',
                xLabel: 'Time (s)',
                yLabel: 'Sensor Telemetry',
                title: 'Live Hardware Telemetry (SIMULATED)',
                emptyMessage: 'Hardware is simulated; no physical device is connected. Select an adapter and click Connect Channel.'
            });
        }

        const deviceSelect = document.getElementById('hw-device-select');
        const btnConnect = document.getElementById('hw-btn-connect');
        const btnDisconnect = document.getElementById('hw-btn-disconnect');
        const btnReadTelemetry = document.getElementById('hw-btn-telemetry');
        const btnRecover = document.getElementById('hw-btn-recover');
        const btnCommand = document.getElementById('hw-btn-send-cmd');

        if (deviceSelect) {
            deviceSelect.addEventListener('change', (e) => {
                this.activeDeviceId = e.target.value;
                this.updateActiveDeviceDetails();
            });
        }

        if (btnConnect) {
            btnConnect.addEventListener('click', () => this.connectDevice());
        }

        if (btnDisconnect) {
            btnDisconnect.addEventListener('click', () => this.disconnectDevice());
        }

        if (btnReadTelemetry) {
            btnReadTelemetry.addEventListener('click', () => this.readTelemetry());
        }

        if (btnRecover) {
            btnRecover.addEventListener('click', () => this.recoverDevice());
        }

        if (btnCommand) {
            btnCommand.addEventListener('click', () => this.sendCommand());
        }

        // Initial device discovery
        this.discoverDevices();
    },

    onActivated() {
        if (this.chart) {
            this.chart.resize();
        }
    },

    async discoverDevices() {
        try {
            const resp = await AppBridge.send('hardware_discover', {});
            if (resp.success && resp.payload && resp.payload.devices) {
                this.renderDeviceCards(resp.payload.devices);
                this.updateActiveDeviceDetails();
            }
        } catch (err) {
            console.error('[HardwareView] Discovery error:', err);
        }
    },

    renderDeviceCards(devices) {
        const container = document.getElementById('hw-device-cards-container');
        if (!container) return;

        container.innerHTML = '';
        devices.forEach(d => {
            const card = document.createElement('div');
            card.className = 'card';
            card.innerHTML = `
                <div class="card-header">
                    <div>
                        <div class="card-title">${d.model_name} <span class="badge badge-simulated">SIMULATED</span></div>
                        <div class="card-subtitle text-mono">${d.device_id} &bull; ${d.physical_interface}</div>
                    </div>
                    <span id="badge-state-${d.device_id}" class="badge ${d.connection_state === 'Connected' ? 'badge-ready' : (d.connection_state === 'Error' ? 'badge-danger' : 'badge-neutral')}">${d.connection_state.toUpperCase()}</span>
                </div>
                <div class="card-body">
                    <div class="grid-2 text-small">
                        <div>Manufacturer: <strong>${d.manufacturer}</strong></div>
                        <div>Firmware: <strong>${d.firmware_version}</strong></div>
                        <div>Serial: <strong class="text-mono">${d.serial_number}</strong></div>
                        <div>Type: <strong>${d.device_type}</strong></div>
                    </div>
                </div>
            `;
            container.appendChild(card);
        });
    },

    updateActiveDeviceDetails() {
        const cmdSelect = document.getElementById('hw-cmd-select');
        if (!cmdSelect) return;

        cmdSelect.innerHTML = '';
        let commands = [];
        if (this.activeDeviceId.includes('stm32')) {
            commands = ['ping', 'flash_firmware', 'read_mcu_registers', 'reset_target'];
        } else if (this.activeDeviceId.includes('basys')) {
            commands = ['ping', 'flash_bitstream', 'read_xadc', 'fpga_reset'];
        } else {
            commands = ['ping', 'jtag_scan_chain', 'set_tck_frequency', 'jtag_shift_dr'];
        }

        commands.forEach(c => {
            const opt = document.createElement('option');
            opt.value = c;
            opt.textContent = c;
            cmdSelect.appendChild(opt);
        });
    },

    async connectDevice() {
        const simTimeout = document.getElementById('hw-toggle-sim-timeout')?.checked || false;
        const simFailure = document.getElementById('hw-toggle-sim-failure')?.checked || false;

        try {
            const resp = await AppBridge.send('hardware_connect', {
                device_id: this.activeDeviceId,
                baud_rate: 115200,
                timeout_ms: 1000,
                simulate_timeout: simTimeout,
                simulate_connection_failure: simFailure
            });

            this.updateDeviceStateBadge(this.activeDeviceId, resp.payload?.connection_state || (resp.success ? 'Connected' : 'Error'));
            if (!resp.success) {
                this.showError(`[SIMULATED FAULT] ${resp.error}`);
            } else {
                this.startTelemetryPolling();
            }
        } catch (err) {
            this.showError(err.message || 'Error communicating with adapter.');
        }
    },

    async disconnectDevice() {
        this.stopTelemetryPolling();
        try {
            const resp = await AppBridge.send('hardware_disconnect', { device_id: this.activeDeviceId });
            this.updateDeviceStateBadge(this.activeDeviceId, resp.payload?.connection_state || 'Disconnected');
        } catch (err) {
            this.showError(err.message || 'Error disconnecting device.');
        }
    },

    async readTelemetry() {
        try {
            const resp = await AppBridge.send('hardware_read_telemetry', { device_id: this.activeDeviceId });
            if (resp.success && resp.payload) {
                this.recordTelemetry(resp.payload);
            } else {
                this.showError(resp.error || 'Failed to read telemetry.');
            }
        } catch (err) {
            this.showError(err.message || 'Error querying telemetry.');
        }
    },

    startTelemetryPolling() {
        this.stopTelemetryPolling();
        this.readTelemetry();
        this.pollTimer = setInterval(() => {
            this.readTelemetry();
        }, 1500);
    },

    stopTelemetryPolling() {
        if (this.pollTimer) {
            clearInterval(this.pollTimer);
            this.pollTimer = null;
        }
    },

    recordTelemetry(t) {
        const timeSec = this.telemetryHistory.length * 1.5;
        this.telemetryHistory.push({
            t: timeSec,
            temp: t.board_temperature_c,
            voltage: t.core_voltage_v,
            power: t.power_consumption_w
        });

        if (this.telemetryHistory.length > 20) {
            this.telemetryHistory.shift();
        }

        // Update Stat Cards
        const elTemp = document.getElementById('hw-stat-temp');
        const elVolt = document.getElementById('hw-stat-volt');
        const elPower = document.getElementById('hw-stat-power');
        const elSeq = document.getElementById('hw-stat-seq');

        if (elTemp) elTemp.textContent = t.board_temperature_c.toFixed(2) + ' °C';
        if (elVolt) elVolt.textContent = t.core_voltage_v.toFixed(3) + ' V';
        if (elPower) elPower.textContent = t.power_consumption_w.toFixed(3) + ' W';
        if (elSeq) elSeq.textContent = '#' + t.sequence_number;

        // Render chart
        if (this.chart) {
            const tempSeries = {
                name: 'Board Temperature (°C) [SIMULATED]',
                color: '#dc2626', // Red
                lineWidth: 2,
                showPoints: true,
                pointRadius: 3,
                data: this.telemetryHistory.map(d => ({ x: d.t, y: d.temp }))
            };

            const voltSeries = {
                name: 'Core Voltage (V) [SIMULATED]',
                color: '#2563eb', // Blue
                lineWidth: 1.5,
                showPoints: true,
                pointRadius: 3,
                data: this.telemetryHistory.map(d => ({ x: d.t, y: d.voltage * 25 })) // scaled for visibility
            };

            const pwrSeries = {
                name: 'Power Consumption (W) [SIMULATED]',
                color: '#16a34a', // Emerald green
                lineWidth: 1.5,
                showPoints: true,
                pointRadius: 3,
                data: this.telemetryHistory.map(d => ({ x: d.t, y: d.power * 25 })) // scaled for visibility
            };

            this.chart.setSeries([tempSeries, voltSeries, pwrSeries]);
        }
    },

    async recoverDevice() {
        // Safe supported recovery: reset simulation flags, then disconnect, then connect clean
        try {
            const resp1 = await AppBridge.send('hardware_reset_simulation_flags', { device_id: this.activeDeviceId });
            if (resp1.success) {
                // Clear UI fault injection toggles
                const simTimeout = document.getElementById('hw-toggle-sim-timeout');
                const simFailure = document.getElementById('hw-toggle-sim-failure');
                if (simTimeout) simTimeout.checked = false;
                if (simFailure) simFailure.checked = false;

                this.updateDeviceStateBadge(this.activeDeviceId, 'Disconnected');
                alert(`Simulation fault flags reset for ${this.activeDeviceId}. Device restored to clean Disconnected state. Click 'Connect' to re-establish channel.`);
            } else {
                this.showError(resp1.error || 'Failed to reset simulation flags.');
            }
        } catch (err) {
            this.showError(err.message || 'Error during recovery.');
        }
    },

    async sendCommand() {
        const cmd = document.getElementById('hw-cmd-select')?.value;
        const payload = document.getElementById('hw-cmd-payload')?.value || '';

        try {
            const resp = await AppBridge.send('hardware_send_command', {
                device_id: this.activeDeviceId,
                command: cmd,
                cmd_payload: payload
            });

            const consoleBox = document.getElementById('hw-console-output');
            if (consoleBox) {
                const ts = new Date().toLocaleTimeString();
                if (resp.success) {
                    consoleBox.innerHTML += `<div>[${ts}] &gt; ${cmd}: <span style="color:#16a34a;">STATUS_OK (SIMULATED)</span></div>`;
                } else {
                    consoleBox.innerHTML += `<div>[${ts}] &gt; ${cmd}: <span style="color:#dc2626;">ERROR: ${resp.error}</span></div>`;
                }
                consoleBox.scrollTop = consoleBox.scrollHeight;
            }
        } catch (err) {
            this.showError(err.message || 'Error executing command.');
        }
    },

    updateDeviceStateBadge(deviceId, state) {
        const badge = document.getElementById(`badge-state-${deviceId}`);
        if (badge) {
            badge.textContent = state.toUpperCase();
            badge.className = `badge ${state === 'Connected' ? 'badge-ready' : (state === 'Error' ? 'badge-danger' : 'badge-neutral')}`;
        }
    },

    showError(msg) {
        const errBox = document.getElementById('hw-error-banner');
        if (errBox) {
            let userMsg = msg;
            if (msg && (msg.includes('NotConnected') || msg.includes('not connected'))) {
                userMsg = 'Target device is disconnected. Click Connect Channel to establish connection.';
            } else if (msg && (msg.includes('Timeout') || msg.includes('timeout'))) {
                userMsg = 'Simulated timeout triggered. Click Reset Fault Flags (Recover) to restore normal state.';
            } else if (msg && (msg.includes('ConnectionFailed') || msg.includes('failed'))) {
                userMsg = 'Simulated connection failure triggered. Click Reset Fault Flags (Recover) to clear simulation fault.';
            }
            errBox.innerHTML = `<strong>Hardware Simulation Notice:</strong> ${userMsg} <span class="text-small text-mono" style="opacity: 0.8; display: block; margin-top: 2px;">Technical details: ${msg}</span>`;
            errBox.style.display = 'block';
            setTimeout(() => { errBox.style.display = 'none'; }, 8000);
        }
    }
};

window.HardwareView = HardwareView;
