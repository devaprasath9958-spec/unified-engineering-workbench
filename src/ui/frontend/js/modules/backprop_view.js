/**
 * UNIFIED ENGINEERING WORKBENCH - BACKPROPAGATION MODULE CONTROLLER
 * Connects UI to C++ Backpropagation & Gradient Descent Engine.
 */

const BackpropView = {
    chart: null,
    lastHistory: null,

    init() {
        const canvas = document.getElementById('backprop-loss-canvas');
        if (canvas) {
            this.chart = new ChartEngine(canvas, {
                type: 'line',
                xLabel: 'Training Step (1 to N)',
                yLabel: 'Quadratic Loss J = 0.5(y_pred - y)²',
                title: 'Gradient Descent Loss Trajectory',
                emptyMessage: 'No training history yet. Click Verify Gradients, then Train 100 Steps to plot loss reduction.'
            });
        }

        const btnVerify = document.getElementById('bp-btn-verify');
        const btnTrain = document.getElementById('bp-btn-train');
        const btnStep = document.getElementById('bp-btn-step');
        const btnReset = document.getElementById('bp-btn-reset');
        const btnExport = document.getElementById('bp-btn-export');

        if (btnVerify) {
            btnVerify.addEventListener('click', () => this.verifyGradients());
        }

        if (btnTrain) {
            btnTrain.addEventListener('click', () => this.train(100));
        }

        if (btnStep) {
            btnStep.addEventListener('click', () => this.stepOnce());
        }

        if (btnReset) {
            btnReset.addEventListener('click', () => this.resetParameters());
        }

        if (btnExport) {
            btnExport.addEventListener('click', () => this.exportCsv());
        }

        // Automatic initial gradient verification and training
        this.verifyGradients();
        this.train(100);
    },

    onActivated() {
        if (this.chart) {
            this.chart.resize();
        }
    },

    getFormValues() {
        return {
            x: parseFloat(document.getElementById('bp-input-x').value) || 1.0,
            y: parseFloat(document.getElementById('bp-input-y').value) || 2.0,
            w: parseFloat(document.getElementById('bp-input-w').value) || 0.5,
            b: parseFloat(document.getElementById('bp-input-b').value) || 0.0,
            v: parseFloat(document.getElementById('bp-input-v').value) || 1.0,
            c: parseFloat(document.getElementById('bp-input-c').value) || 0.0,
            learning_rate: parseFloat(document.getElementById('bp-input-eta').value) || 0.1
        };
    },

    resetParameters() {
        document.getElementById('bp-input-x').value = '1.0';
        document.getElementById('bp-input-y').value = '2.0';
        document.getElementById('bp-input-w').value = '0.5';
        document.getElementById('bp-input-b').value = '0.0';
        document.getElementById('bp-input-v').value = '1.0';
        document.getElementById('bp-input-c').value = '0.0';
        document.getElementById('bp-input-eta').value = '0.1';
        this.train(100);
    },

    async verifyGradients() {
        const p = this.getFormValues();
        try {
            const resp = await AppBridge.send('backprop_verify_gradients', {
                x: p.x, y: p.y, w: p.w, b: p.b, v: p.v, c: p.c,
                epsilon: 1.0e-6, tolerance: 1.0e-5
            });

            if (resp.success && resp.payload) {
                const res = resp.payload;
                const elBadge = document.getElementById('bp-verif-badge');
                const elMaxErr = document.getElementById('bp-verif-maxerr');

                if (elBadge) {
                    elBadge.textContent = res.passed ? 'CHAIN RULE VERIFIED' : 'VERIFICATION FAILED';
                    elBadge.className = `badge ${res.passed ? 'badge-ready' : 'badge-danger'}`;
                }
                if (elMaxErr) {
                    elMaxErr.textContent = `Max Error: ${res.max_absolute_error.toExponential(4)} (Tol: ${res.tolerance.toExponential(1)})`;
                }

                this.updateGradientTable(res);
            } else {
                this.showError(resp.error || 'Gradient verification failed.');
            }
        } catch (err) {
            this.showError(err.message || 'Error verifying gradients.');
        }
    },

    updateGradientTable(res) {
        const tbody = document.getElementById('bp-gradient-table-body');
        if (!tbody) return;

        tbody.innerHTML = `
            <tr>
                <td><strong>dJ/dw</strong></td>
                <td class="cell-numeric">${res.analytical.dw.toFixed(8)}</td>
                <td class="cell-numeric">${res.numerical.dw.toFixed(8)}</td>
                <td class="cell-numeric">${res.absolute_errors.dw.toExponential(4)}</td>
            </tr>
            <tr>
                <td><strong>dJ/db</strong></td>
                <td class="cell-numeric">${res.analytical.db.toFixed(8)}</td>
                <td class="cell-numeric">${res.numerical.db.toFixed(8)}</td>
                <td class="cell-numeric">${res.absolute_errors.db.toExponential(4)}</td>
            </tr>
            <tr>
                <td><strong>dJ/dv</strong></td>
                <td class="cell-numeric">${res.analytical.dv.toFixed(8)}</td>
                <td class="cell-numeric">${res.numerical.dv.toFixed(8)}</td>
                <td class="cell-numeric">${res.absolute_errors.dv.toExponential(4)}</td>
            </tr>
            <tr>
                <td><strong>dJ/dc</strong></td>
                <td class="cell-numeric">${res.analytical.dc.toFixed(8)}</td>
                <td class="cell-numeric">${res.numerical.dc.toFixed(8)}</td>
                <td class="cell-numeric">${res.absolute_errors.dc.toExponential(4)}</td>
            </tr>
        `;
    },

    async train(steps = 100) {
        const p = this.getFormValues();
        this.showLoading(true);

        try {
            const resp = await AppBridge.send('backprop_train', {
                x: p.x, y: p.y, w: p.w, b: p.b, v: p.v, c: p.c,
                learning_rate: p.learning_rate, steps: steps
            });

            if (resp.success && resp.payload) {
                this.lastHistory = resp.payload;
                this.renderLossCurve(this.lastHistory.records);
                this.updateHistoryTable(this.lastHistory.records);
                this.updateTrainingStats(this.lastHistory);
            } else {
                this.showError(resp.error || 'Training failed.');
            }
        } catch (err) {
            this.showError(err.message || 'Error running gradient descent.');
        } finally {
            this.showLoading(false);
        }
    },

    async stepOnce() {
        const p = this.getFormValues();
        try {
            const resp = await AppBridge.send('backprop_step', {
                x: p.x, y: p.y, w: p.w, b: p.b, v: p.v, c: p.c,
                learning_rate: p.learning_rate
            });

            if (resp.success && resp.payload) {
                const updated = resp.payload.updated_parameters;
                document.getElementById('bp-input-w').value = updated.w.toFixed(6);
                document.getElementById('bp-input-b').value = updated.b.toFixed(6);
                document.getElementById('bp-input-v').value = updated.v.toFixed(6);
                document.getElementById('bp-input-c').value = updated.c.toFixed(6);

                alert(`Step complete! Loss: ${resp.payload.loss.toFixed(6)}, y_pred: ${resp.payload.y_pred.toFixed(4)}`);
            } else {
                this.showError(resp.error || 'Single step failed.');
            }
        } catch (err) {
            this.showError(err.message || 'Error performing step.');
        }
    },

    renderLossCurve(records) {
        if (!this.chart || !records) return;

        const lossData = records.map(r => ({ x: r.step, y: r.loss }));
        this.chart.setSeries([{
            name: 'Quadratic Loss J',
            color: '#2563eb',
            lineWidth: 2,
            showPoints: false,
            data: lossData
        }]);
    },

    updateHistoryTable(records) {
        const tbody = document.getElementById('bp-history-table-body');
        if (!tbody || !records) return;

        tbody.innerHTML = '';
        // Show first 10, middle samples, and last 10
        const displayRecords = records.length <= 25 ? records : [
            ...records.slice(0, 8),
            records[Math.floor(records.length / 2)],
            ...records.slice(-8)
        ];

        records.slice(0, 15).forEach(r => {
            const tr = document.createElement('tr');
            tr.innerHTML = `
                <td class="cell-numeric">#${r.step}</td>
                <td class="cell-numeric">${r.loss.toExponential(6)}</td>
                <td class="cell-numeric">${r.y_pred.toFixed(6)}</td>
                <td class="cell-numeric">${r.w.toFixed(5)}</td>
                <td class="cell-numeric">${r.b.toFixed(5)}</td>
                <td class="cell-numeric">${r.v.toFixed(5)}</td>
                <td class="cell-numeric">${r.c.toFixed(5)}</td>
            `;
            tbody.appendChild(tr);
        });
    },

    updateTrainingStats(history) {
        const elInit = document.getElementById('bp-stat-init-loss');
        const elFinal = document.getElementById('bp-stat-final-loss');
        const elSteps = document.getElementById('bp-stat-steps');

        if (elInit) elInit.textContent = history.initial_loss.toFixed(6);
        if (elFinal) elFinal.textContent = history.final_loss < 1e-4 ? history.final_loss.toExponential(4) : history.final_loss.toFixed(6);
        if (elSteps) elSteps.textContent = history.step_count.toString();
    },

    async exportCsv() {
        try {
            const resp = await AppBridge.send('backprop_export_csv', { filepath: 'results/backprop/loss_history.csv' });
            if (resp.success) {
                if (window.ToastManager && typeof ToastManager.show === 'function') {
                    ToastManager.show('success', 'Export Completed', 'Training loss history exported to results/backprop/loss_history.csv');
                }
            } else {
                this.showError(resp.error || 'CSV export failed.');
            }
        } catch (err) {
            this.showError(err.message || 'Error exporting CSV.');
        }
    },

    showLoading(loading) {
        const spinner = document.getElementById('bp-loading-indicator');
        if (spinner) spinner.style.display = loading ? 'inline-block' : 'none';
    },

    showError(msg) {
        const errBox = document.getElementById('bp-error-banner');
        if (errBox) {
            let userMsg = msg;
            if (msg && (msg.includes('learning_rate') || msg.includes('strictly positive'))) {
                userMsg = 'Learning rate must be strictly positive (eta > 0).';
            }
            errBox.innerHTML = `<strong>Backpropagation Error:</strong> ${userMsg} <span class="text-small text-mono" style="opacity: 0.8; display: block; margin-top: 2px;">Technical details: ${msg}</span>`;
            errBox.style.display = 'block';
            setTimeout(() => { errBox.style.display = 'none'; }, 8000);
        }
    }
};

window.BackpropView = BackpropView;
