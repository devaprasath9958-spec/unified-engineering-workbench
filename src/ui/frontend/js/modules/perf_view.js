/**
 * UNIFIED ENGINEERING WORKBENCH - MULTICORE PERFORMANCE BENCHMARK CONTROLLER
 * Connects UI benchmark controls to C++ Parallel Benchmarking Engine.
 * Implements honest indeterminate progress indicators (zero fabricated percentages).
 */

const PerfView = {
    chart: null,
    lastResult: null,

    init() {
        const canvas = document.getElementById('perf-chart-canvas');
        if (canvas) {
            this.chart = new ChartEngine(canvas, {
                type: 'bar',
                xLabel: 'Worker Concurrency Mode',
                yLabel: 'Speedup Multiplier (T_serial / T_parallel)',
                title: 'Multicore Scaling & Efficiency',
                emptyMessage: 'No benchmark results yet. Click Run Multicore Benchmark to measure execution times and plot scaling curve.'
            });
        }

        const btnRun = document.getElementById('perf-btn-run');
        const btnExportRaw = document.getElementById('perf-btn-export-raw');
        const btnExportSummary = document.getElementById('perf-btn-export-summary');

        if (btnRun) {
            btnRun.addEventListener('click', () => this.runBenchmark());
        }

        if (btnExportRaw) {
            btnExportRaw.addEventListener('click', () => this.exportCsv('raw'));
        }

        if (btnExportSummary) {
            btnExportSummary.addEventListener('click', () => this.exportCsv('summary'));
        }

        // Fetch real host hardware metadata
        this.fetchHardwareInfo();
    },

    onActivated() {
        if (this.chart) {
            this.chart.resize();
        }
    },

    async fetchHardwareInfo() {
        try {
            const resp = await AppBridge.send('benchmark_get_hardware_info', {});
            if (resp.success && resp.payload) {
                const info = resp.payload;
                const elCpu = document.getElementById('perf-stat-cpu');
                const elCores = document.getElementById('perf-stat-cores');
                const elThreads = document.getElementById('perf-stat-threads');
                const elRam = document.getElementById('perf-stat-ram');

                if (elCpu) elCpu.textContent = info.cpu_model || 'Standard x64 Processor';
                if (elCores) elCores.textContent = info.physical_core_count ? info.physical_core_count.toString() : 'Detected';
                if (elThreads) elThreads.textContent = info.logical_processor_count ? info.logical_processor_count.toString() : 'Available';
                if (elRam) elRam.textContent = info.total_ram_readable || 'Available';
            }
        } catch (err) {
            console.error('[PerfView] Hardware info query error:', err);
        }
    },

    getWorkloadParameters() {
        const cases = parseInt(document.getElementById('perf-input-cases').value, 10) || 100;
        const elements = parseInt(document.getElementById('perf-input-elements').value, 10) || 80;
        const seed = parseInt(document.getElementById('perf-input-seed').value, 10) || 42;

        const workerBoxes = document.querySelectorAll('input[name="perf-worker"]:checked');
        let workerTargets = Array.from(workerBoxes).map(cb => parseInt(cb.value, 10));
        if (workerTargets.length === 0) workerTargets = [1, 2, 3, 4];

        return {
            case_count: cases,
            elements_per_case: elements,
            random_seed: seed,
            worker_targets: workerTargets
        };
    },

    async runBenchmark() {
        const params = this.getWorkloadParameters();
        this.setRunningState(true);

        try {
            const resp = await AppBridge.send('benchmark_run', params);
            if (resp.success && resp.payload) {
                this.lastResult = resp.payload;
                this.renderBarChart(this.lastResult);
                this.updateSummaryTable(this.lastResult.summaries);
                this.updateRawTable(this.lastResult.raw_records);
                this.updateOverallStatus(this.lastResult);
            } else {
                this.showError(resp.error || 'Benchmark suite execution failed.');
            }
        } catch (err) {
            this.showError(err.message || 'Error communicating with host benchmark engine.');
        } finally {
            this.setRunningState(false);
        }
    },

    renderBarChart(res) {
        if (!this.chart || !res.summaries) return;

        const barData = res.summaries.map(s => ({
            label: s.actual_workers === 1 ? '1 Worker (Serial)' : `${s.actual_workers} Workers`,
            workers: s.actual_workers,
            speedup: s.speedup,
            ideal: s.actual_workers * 1.0
        }));

        this.chart.setSeries([{
            name: 'Scaling Comparison',
            data: barData
        }]);
    },

    updateSummaryTable(summaries) {
        const tbody = document.getElementById('perf-summary-table-body');
        if (!tbody || !summaries) return;

        tbody.innerHTML = '';
        summaries.forEach(s => {
            const tr = document.createElement('tr');
            tr.innerHTML = `
                <td><strong>${s.execution_mode}</strong></td>
                <td class="cell-numeric">${s.actual_workers}</td>
                <td class="cell-numeric">${s.median_time_ms.toFixed(2)} ms</td>
                <td class="cell-numeric"><strong>${s.speedup.toFixed(3)}x</strong></td>
                <td class="cell-numeric">${s.efficiency_percent.toFixed(1)}%</td>
                <td><span class="badge ${s.all_repetitions_correct ? 'badge-ready' : 'badge-danger'}">${s.all_repetitions_correct ? 'PASS (<= 1e-8 °C)' : 'FAIL'}</span></td>
            `;
            tbody.appendChild(tr);
        });
    },

    updateRawTable(records) {
        const tbody = document.getElementById('perf-raw-table-body');
        if (!tbody || !records) return;

        tbody.innerHTML = '';
        records.forEach(r => {
            const tr = document.createElement('tr');
            tr.innerHTML = `
                <td>${r.execution_mode}</td>
                <td class="cell-numeric">#${r.repetition_index}</td>
                <td class="cell-numeric">${r.elapsed_time_ms.toFixed(2)} ms</td>
                <td class="cell-numeric">${r.max_temperature_difference_C.toExponential(3)} °C</td>
                <td><span class="badge ${r.correctness_passed ? 'badge-ready' : 'badge-danger'}">${r.correctness_passed ? 'CORRECT' : 'DIVERGED'}</span></td>
            `;
            tbody.appendChild(tr);
        });
    },

    updateOverallStatus(res) {
        const elStatus = document.getElementById('perf-overall-status');
        if (elStatus) {
            if (res.overall_correctness_passed) {
                elStatus.innerHTML = `<span class="badge badge-ready">VERIFIED</span> Serial and Parallel solutions agree across all repetitions (&Delta;T &le; 10⁻⁸ °C).`;
            } else {
                elStatus.innerHTML = `<span class="badge badge-danger">FAILED</span> Numerical divergence detected between serial and parallel executions.`;
            }
        }
    },

    async exportCsv(type) {
        try {
            const resp = await AppBridge.send('benchmark_export_csv', {
                raw_path: 'results/benchmark/benchmark_raw.csv',
                summary_path: 'results/benchmark/benchmark_summary.csv'
            });
            if (resp.success) {
                if (window.ToastManager && typeof ToastManager.show === 'function') {
                    ToastManager.show('success', 'Export Completed', `Benchmark ${type} dataset exported to results/benchmark/`);
                }
            } else {
                this.showError(resp.error || 'CSV export failed.');
            }
        } catch (err) {
            this.showError(err.message || 'Error exporting CSV.');
        }
    },

    setRunningState(running) {
        const btnRun = document.getElementById('perf-btn-run');
        const spinner = document.getElementById('perf-loading-indicator');
        const statusBox = document.getElementById('perf-running-notice');

        if (btnRun) {
            btnRun.disabled = running;
            btnRun.textContent = running ? 'Benchmarking in Progress...' : 'Run Multicore Benchmark';
        }
        if (spinner) spinner.style.display = running ? 'inline-block' : 'none';
        if (statusBox) {
            statusBox.style.display = running ? 'block' : 'none';
            if (running) {
                statusBox.textContent = 'Benchmark Suite Running: Executing 1 warm-up + 5 measured repetitions across worker configurations on background thread. UI remains responsive.';
            }
        }
    },

    showError(msg) {
        const errBox = document.getElementById('perf-error-banner');
        if (errBox) {
            let userMsg = msg;
            if (msg && (msg.includes('case count') || msg.includes('elements'))) {
                userMsg = 'Invalid workload parameter. Ensure Case Count >= 1 and Elements >= 10.';
            }
            errBox.innerHTML = `<strong>Benchmark Error:</strong> ${userMsg} <span class="text-small text-mono" style="opacity: 0.8; display: block; margin-top: 2px;">Technical details: ${msg}</span>`;
            errBox.style.display = 'block';
            setTimeout(() => { errBox.style.display = 'none'; }, 8000);
        }
    }
};

window.PerfView = PerfView;
