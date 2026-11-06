/**
 * UNIFIED ENGINEERING WORKBENCH - THERMAL FEM MODULE CONTROLLER
 * Connects UI parameter controls and plot to C++ 1D Thermal FEM Solver.
 */

const ThermalView = {
    chart: null,
    lastResult: null,

    init() {
        const canvas = document.getElementById('thermal-chart-canvas');
        if (!canvas) return;

        this.chart = new ChartEngine(canvas, {
            type: 'line',
            xLabel: 'Position x (m)',
            yLabel: 'Temperature T (°C)',
            title: '1D Steady-State Heat Conduction',
            emptyMessage: 'No thermal solution available. Set rod parameters and click Solve FEM to plot temperature profile.'
        });

        const btnSolve = document.getElementById('thermal-btn-solve');
        const btnReset = document.getElementById('thermal-btn-reset');
        const btnExport = document.getElementById('thermal-btn-export');

        if (btnSolve) {
            btnSolve.addEventListener('click', () => this.solve());
        }

        if (btnReset) {
            btnReset.addEventListener('click', () => this.resetDefaults());
        }

        if (btnExport) {
            btnExport.addEventListener('click', () => this.exportCsv());
        }

        // Trigger initial solve
        this.solve();
    },

    onActivated() {
        if (this.chart) {
            this.chart.resize();
        }
    },

    getFormParameters() {
        return {
            length: parseFloat(document.getElementById('thermal-input-length').value) || 1.0,
            area: parseFloat(document.getElementById('thermal-input-area').value) || 1e-4,
            thermal_conductivity: parseFloat(document.getElementById('thermal-input-k').value) || 10.0,
            heat_generation: parseFloat(document.getElementById('thermal-input-q').value) || 10000.0,
            left_temperature: parseFloat(document.getElementById('thermal-input-t0').value) || 100.0,
            right_temperature: parseFloat(document.getElementById('thermal-input-tl').value) || 20.0,
            element_count: parseInt(document.getElementById('thermal-input-n').value, 10) || 10
        };
    },

    resetDefaults() {
        document.getElementById('thermal-input-length').value = '1.0';
        document.getElementById('thermal-input-area').value = '0.0001';
        document.getElementById('thermal-input-k').value = '10.0';
        document.getElementById('thermal-input-q').value = '10000.0';
        document.getElementById('thermal-input-t0').value = '100.0';
        document.getElementById('thermal-input-tl').value = '20.0';
        document.getElementById('thermal-input-n').value = '10';
        this.solve();
    },

    async solve() {
        const params = this.getFormParameters();
        this.showLoading(true);

        try {
            const resp = await AppBridge.send('thermal_solve', params);
            if (resp.success && resp.payload) {
                this.lastResult = resp.payload;
                this.renderPlot(this.lastResult);
                this.updateTable(this.lastResult.nodes);
                this.updateMetrics(this.lastResult);
            } else {
                this.showError(resp.error || 'Thermal solve failed.');
            }
        } catch (err) {
            this.showError(err.message || 'Error communicating with host bridge.');
        } finally {
            this.showLoading(false);
        }
    },

    renderPlot(res) {
        const exactSeries = {
            name: 'Exact Analytical Solution',
            color: '#64748b', // Slate grey
            lineWidth: 2,
            showPoints: false,
            data: res.exact_curve.map(pt => ({ x: pt.x_m, y: pt.temperature_exact_C }))
        };

        const femSeries = {
            name: 'FEM Nodal Solution',
            color: '#2563eb', // Royal blue
            lineWidth: 1.5,
            showPoints: true,
            pointRadius: 4,
            data: res.nodes.map(n => ({ x: n.x_m, y: n.temperature_FE_C }))
        };

        this.chart.setSeries([exactSeries, femSeries]);
    },

    updateTable(nodes) {
        const tbody = document.getElementById('thermal-table-body');
        if (!tbody) return;

        tbody.innerHTML = '';
        nodes.forEach(n => {
            const tr = document.createElement('tr');
            tr.innerHTML = `
                <td class="cell-numeric">${n.node_index}</td>
                <td class="cell-numeric">${n.x_m.toFixed(4)}</td>
                <td class="cell-numeric">${n.temperature_FE_C.toFixed(4)}</td>
                <td class="cell-numeric">${n.temperature_exact_C.toFixed(4)}</td>
                <td class="cell-numeric">${n.absolute_error_C.toExponential(4)}</td>
            `;
            tbody.appendChild(tr);
        });
    },

    updateMetrics(res) {
        const elH = document.getElementById('thermal-stat-h');
        const elMaxErr = document.getElementById('thermal-stat-max-err');
        const elExpErr = document.getElementById('thermal-stat-exp-err');
        const elBadge = document.getElementById('thermal-convergence-badge');

        if (elH) elH.textContent = res.element_length.toFixed(4) + ' m';
        if (elMaxErr) elMaxErr.textContent = res.max_absolute_midpoint_error_C.toFixed(6) + ' °C';
        if (elExpErr) elExpErr.textContent = res.expected_max_midpoint_error_C.toFixed(6) + ' °C';

        if (elBadge) {
            const diff = Math.abs(res.max_absolute_midpoint_error_C - res.expected_max_midpoint_error_C);
            if (diff < 1e-10) {
                elBadge.textContent = 'O(h²) VERIFIED';
                elBadge.className = 'badge badge-ready';
            } else {
                elBadge.textContent = 'CONVERGENCE CHECK';
                elBadge.className = 'badge badge-warning';
            }
        }
    },

    async exportCsv() {
        try {
            const resp = await AppBridge.send('thermal_export_csv', { filepath: 'results/thermal/thermal_results.csv' });
            if (resp.success) {
                if (window.ToastManager && typeof ToastManager.show === 'function') {
                    ToastManager.show('success', 'Export Completed', 'Thermal FEM results successfully exported to results/thermal/thermal_results.csv');
                }
            } else {
                this.showError(resp.error || 'CSV export failed.');
            }
        } catch (err) {
            this.showError(err.message || 'Error exporting CSV.');
        }
    },

    showLoading(loading) {
        const spinner = document.getElementById('thermal-loading-indicator');
        if (spinner) spinner.style.display = loading ? 'inline-block' : 'none';
    },

    showError(msg) {
        const errBox = document.getElementById('thermal-error-banner');
        if (errBox) {
            let userMsg = msg;
            if (msg && (msg.includes('strictly positive') || msg.includes('positive') || msg.includes('length'))) {
                userMsg = 'Parameters must be positive non-zero values (L > 0, A > 0, k > 0, N >= 1).';
            }
            errBox.innerHTML = `<strong>Thermal Solver Error:</strong> ${userMsg} <span class="text-small text-mono" style="opacity: 0.8; display: block; margin-top: 2px;">Technical details: ${msg}</span>`;
            errBox.style.display = 'block';
            setTimeout(() => { errBox.style.display = 'none'; }, 8000);
        }
    }
};

window.ThermalView = ThermalView;
