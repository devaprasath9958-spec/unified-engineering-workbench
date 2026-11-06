/**
 * UNIFIED ENGINEERING WORKBENCH - GEOMETRY MODULE CONTROLLER
 * Connects UI controls to C++ geometry parsers via HostBridge.
 */

const GeometryView = {
    canvas3D: null,
    currentMesh: null,
    currentField: null,

    init() {
        const canvas = document.getElementById('geometry-canvas-3d');
        if (!canvas) return;

        this.canvas3D = new Canvas3D(canvas);

        // Controls
        const sampleSelect = document.getElementById('geo-sample-select');
        const overlaySelect = document.getElementById('geo-overlay-select');
        const btnResetCam = document.getElementById('geo-btn-reset-cam');
        const modeRadios = document.querySelectorAll('input[name="geo-render-mode"]');

        if (sampleSelect) {
            sampleSelect.addEventListener('change', (e) => {
                if (e.target.value) {
                    this.loadSampleStl(e.target.value);
                }
            });
        }

        if (overlaySelect) {
            overlaySelect.addEventListener('change', (e) => {
                if (e.target.value) {
                    this.loadSampleCsv(e.target.value);
                } else {
                    this.canvas3D.setTemperatureField(null);
                    this.updatePointStats(null);
                }
            });
        }

        if (btnResetCam) {
            btnResetCam.addEventListener('click', () => {
                this.canvas3D.resetCamera();
            });
        }

        modeRadios.forEach(radio => {
            radio.addEventListener('change', (e) => {
                if (e.target.checked) {
                    this.canvas3D.setMode(e.target.value);
                }
            });
        });

        // Load default sample
        this.loadSampleStl('bracket');
    },

    onActivated() {
        if (this.canvas3D) {
            this.canvas3D.resize();
        }
        if (!this.currentMesh) {
            this.loadSampleStl('bracket');
        }
    },

    async loadSampleStl(sampleKey) {
        this.showLoading(true);
        try {
            const resp = await AppBridge.send('geometry_load_sample', { sample_key: sampleKey });
            let payload = resp ? resp.payload : null;
            if (typeof payload === 'string') {
                try { payload = JSON.parse(payload); } catch (e) {}
            }
            if (resp && resp.success && payload) {
                this.currentMesh = payload;
                this.canvas3D.setMesh(this.currentMesh);
                this.updateMeshStats(this.currentMesh);
            } else {
                this.showError((resp && resp.error) || 'Failed to load STL model.');
            }
        } catch (err) {
            this.showError(err.message || 'Error communicating with host bridge.');
        } finally {
            this.showLoading(false);
        }
    },

    async loadSampleCsv(sampleKey) {
        this.showLoading(true);
        try {
            const resp = await AppBridge.send('geometry_load_sample', { sample_key: sampleKey });
            let payload = resp ? resp.payload : null;
            if (typeof payload === 'string') {
                try { payload = JSON.parse(payload); } catch (e) {}
            }
            if (resp && resp.success && payload) {
                this.currentField = payload;
                this.canvas3D.setTemperatureField(this.currentField);
                this.updatePointStats(this.currentField);
            } else {
                this.showError((resp && resp.error) || 'Failed to load CSV temperature field.');
            }
        } catch (err) {
            this.showError(err.message || 'Error communicating with host bridge.');
        } finally {
            this.showLoading(false);
        }
    },

    updateMeshStats(mesh) {
        const elSolid = document.getElementById('geo-stat-solid');
        const elTriangles = document.getElementById('geo-stat-triangles');
        const elArea = document.getElementById('geo-stat-area');
        const elDegenerate = document.getElementById('geo-stat-degenerate');
        const elSimplification = document.getElementById('geo-simplification-notice');

        if (elSolid) elSolid.textContent = mesh.solid_name || 'Unnamed Solid';
        if (elTriangles) elTriangles.textContent = `${mesh.rendered_triangles.toLocaleString()} (${mesh.total_triangles.toLocaleString()} total)`;
        if (elArea) elArea.textContent = mesh.total_surface_area.toFixed(6) + ' m²';
        if (elDegenerate) {
            elDegenerate.textContent = mesh.degenerate_count.toString();
            elDegenerate.className = mesh.degenerate_count > 0 ? 'badge badge-warning' : 'badge badge-ready';
        }

        if (elSimplification) {
            if (mesh.simplified_visualization) {
                elSimplification.textContent = mesh.simplification_label;
                elSimplification.style.display = 'block';
            } else {
                elSimplification.style.display = 'none';
            }
        }
    },

    updatePointStats(field) {
        const elNodes = document.getElementById('geo-stat-nodes');
        const elMinTemp = document.getElementById('geo-stat-mintemp');
        const elMaxTemp = document.getElementById('geo-stat-maxtemp');
        const elAvgTemp = document.getElementById('geo-stat-avgtemp');

        if (!field) {
            if (elNodes) elNodes.textContent = 'None';
            if (elMinTemp) elMinTemp.textContent = '--';
            if (elMaxTemp) elMaxTemp.textContent = '--';
            if (elAvgTemp) elAvgTemp.textContent = '--';
            return;
        }

        if (elNodes) elNodes.textContent = field.node_count.toLocaleString();
        if (elMinTemp) elMinTemp.textContent = field.min_temperature.toFixed(2) + ' °C';
        if (elMaxTemp) elMaxTemp.textContent = field.max_temperature.toFixed(2) + ' °C';
        if (elAvgTemp) elAvgTemp.textContent = field.average_temperature.toFixed(2) + ' °C';
    },

    showLoading(loading) {
        const spinner = document.getElementById('geo-loading-indicator');
        if (spinner) spinner.style.display = loading ? 'inline-block' : 'none';
    },

    showError(msg) {
        const errBox = document.getElementById('geo-error-banner');
        if (errBox) {
            let userMsg = msg;
            if (!msg || msg.includes('empty') || msg.includes('0 facets') || msg.includes('no solid')) {
                userMsg = 'STL file is empty or contains no valid facets. Select a valid ASCII STL file.';
            } else if (msg.includes('FileNotFound') || msg.includes('Cannot find') || msg.includes('does not exist')) {
                userMsg = 'Geometry dataset file not found. Ensure the data/samples directory is present.';
            }
            errBox.innerHTML = `<strong>Error:</strong> ${userMsg} <span class="text-small text-mono" style="opacity: 0.8; display: block; margin-top: 2px;">Technical details: ${msg}</span>`;
            errBox.style.display = 'block';
            setTimeout(() => { errBox.style.display = 'none'; }, 8000);
        }
    }
};

window.GeometryView = GeometryView;
