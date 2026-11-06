/**
 * UNIFIED ENGINEERING WORKBENCH - CHART ENGINE
 * Pure HTML5 Canvas 2D Engineering Plot Renderer (Zero external dependencies).
 * Supports continuous curves, nodal discrete markers, bar charts, gridlines,
 * HiDPI displays, theme reactivity (light & dark mode), and interactive tooltips.
 */

class ChartEngine {
    static instances = new Set();

    static redrawAll() {
        ChartEngine.instances.forEach(chart => {
            chart.updateTheme();
            chart.resize();
        });
    }

    constructor(canvas, options = {}) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.options = Object.assign({
            type: 'line', // 'line' or 'bar'
            title: '',
            xLabel: 'X',
            yLabel: 'Y',
            emptyMessage: 'No data to display',
            padding: { top: 32, right: 32, bottom: 48, left: 68 },
            fontFamily: 'Segoe UI, system-ui, -apple-system, sans-serif',
            fontSize: 11,
            showLegend: true
        }, options);

        this.series = [];
        this.hoverPoint = null;
        this.isHovering = false;
        this.theme = null;

        ChartEngine.instances.add(this);

        this.updateTheme();
        this.initEvents();
        this.initObserver();
        this.resize();
    }

    destroy() {
        if (this.resizeObserver) {
            this.resizeObserver.disconnect();
            this.resizeObserver = null;
        }
        ChartEngine.instances.delete(this);
    }

    getThemeColors() {
        const isDark = document.documentElement.getAttribute('data-theme') === 'dark';
        if (isDark) {
            return {
                bg: '#202020',
                gridColor: '#2e2e2e',
                axisColor: '#484848',
                textColor: '#a3a3a3',
                titleColor: '#ffffff',
                tooltipBg: 'rgba(23, 23, 23, 0.95)',
                tooltipText: '#ffffff',
                tooltipBorder: '#525252',
                idealBarColor: '#525252',
                idealTextColor: '#a3a3a3',
                barValueColor: '#ffffff',
                emptyColor: '#737373'
            };
        } else {
            return {
                bg: '#ffffff',
                gridColor: '#eeeeee',
                axisColor: '#d4d4d4',
                textColor: '#737373',
                titleColor: '#171717',
                tooltipBg: 'rgba(23, 23, 23, 0.92)',
                tooltipText: '#ffffff',
                tooltipBorder: '#383838',
                idealBarColor: '#94a3b8',
                idealTextColor: '#64748b',
                barValueColor: '#171717',
                emptyColor: '#8c8c8c'
            };
        }
    }

    updateTheme() {
        this.theme = this.getThemeColors();
    }

    initEvents() {
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.canvas.addEventListener('mouseleave', () => this.onMouseLeave());
        window.addEventListener('resize', () => this.resize());
    }

    initObserver() {
        if (window.ResizeObserver) {
            this.resizeObserver = new ResizeObserver((entries) => {
                for (let entry of entries) {
                    if (entry.contentRect.width > 0 && entry.contentRect.height > 0) {
                        this.resize();
                    }
                }
            });
            this.resizeObserver.observe(this.canvas);
        }
    }

    resize() {
        const rect = this.canvas.getBoundingClientRect();
        if (rect.width === 0 || rect.height === 0) return;

        const dpr = window.devicePixelRatio || 1;
        this.canvas.width = Math.round(rect.width * dpr);
        this.canvas.height = Math.round(rect.height * dpr);
        if (this.ctx.resetTransform) {
            this.ctx.resetTransform();
        } else {
            this.ctx.setTransform(1, 0, 0, 1, 0, 0);
        }
        this.ctx.scale(dpr, dpr);
        this.width = rect.width;
        this.height = rect.height;
        this.render();
    }

    setSeries(series) {
        this.series = series || [];
        this.render();
    }

    onMouseMove(e) {
        if (!this.series || this.series.length === 0 || this.options.type === 'bar') return;
        const rect = this.canvas.getBoundingClientRect();
        const mouseX = e.clientX - rect.left;
        const mouseY = e.clientY - rect.top;

        const pad = this.options.padding;
        const plotW = this.width - pad.left - pad.right;
        const plotH = this.height - pad.top - pad.bottom;

        if (mouseX < pad.left || mouseX > pad.left + plotW ||
            mouseY < pad.top || mouseY > pad.top + plotH) {
            if (this.isHovering) {
                this.isHovering = false;
                this.hoverPoint = null;
                this.render();
            }
            return;
        }

        // Find nearest point across all series
        let nearest = null;
        let minDist = 25; // 25px radius

        const bounds = this.computeBounds();
        const toCanvas = (x, y) => ({
            cx: pad.left + ((x - bounds.minX) / (bounds.maxX - bounds.minX || 1)) * plotW,
            cy: pad.top + plotH - ((y - bounds.minY) / (bounds.maxY - bounds.minY || 1)) * plotH
        });

        this.series.forEach(s => {
            if (!s.data) return;
            s.data.forEach(pt => {
                const { cx, cy } = toCanvas(pt.x, pt.y);
                const d = Math.hypot(mouseX - cx, mouseY - cy);
                if (d < minDist) {
                    minDist = d;
                    nearest = { seriesName: s.name, color: s.color || '#2563eb', x: pt.x, y: pt.y, cx, cy };
                }
            });
        });

        if (nearest) {
            this.hoverPoint = nearest;
            this.isHovering = true;
            this.render();
        } else if (this.isHovering) {
            this.hoverPoint = null;
            this.isHovering = false;
            this.render();
        }
    }

    onMouseLeave() {
        if (this.isHovering) {
            this.isHovering = false;
            this.hoverPoint = null;
            this.render();
        }
    }

    computeBounds() {
        let minX = Infinity, maxX = -Infinity;
        let minY = Infinity, maxY = -Infinity;

        this.series.forEach(s => {
            if (!s.data) return;
            s.data.forEach(pt => {
                if (pt.x < minX) minX = pt.x;
                if (pt.x > maxX) maxX = pt.x;
                if (pt.y < minY) minY = pt.y;
                if (pt.y > maxY) maxY = pt.y;
            });
        });

        if (!isFinite(minX)) { minX = 0; maxX = 1; }
        if (!isFinite(minY)) { minY = 0; maxY = 1; }

        if (minX === maxX) { minX -= 0.5; maxX += 0.5; }
        if (minY === maxY) { minY -= 0.5; maxY += 0.5; }

        const ySpan = maxY - minY;
        if (minY >= 0 && minY - ySpan * 0.05 < 0) {
            minY = 0;
        } else {
            minY -= ySpan * 0.05;
        }
        maxY += ySpan * 0.05;

        return { minX, maxX, minY, maxY };
    }

    render() {
        if (!this.width || !this.height) return;
        const ctx = this.ctx;
        ctx.clearRect(0, 0, this.width, this.height);

        const pad = this.options.padding;
        const plotW = this.width - pad.left - pad.right;
        const plotH = this.height - pad.top - pad.bottom;

        if (plotW <= 0 || plotH <= 0) return;

        if (!this.theme) this.updateTheme();
        const th = this.theme;

        // Render card background matching active theme
        ctx.fillStyle = th.bg;
        ctx.fillRect(0, 0, this.width, this.height);

        // Check for empty data state
        const hasData = this.series && this.series.length > 0 &&
                        this.series.some(s => s.data && s.data.length > 0);

        if (!hasData) {
            this.renderEmptyState(ctx, pad, plotW, plotH, th);
            return;
        }

        if (this.options.type === 'bar') {
            this.renderBarChart(ctx, pad, plotW, plotH, th);
            return;
        }

        const bounds = this.computeBounds();
        const toX = (x) => pad.left + ((x - bounds.minX) / (bounds.maxX - bounds.minX || 1)) * plotW;
        const toY = (y) => pad.top + plotH - ((y - bounds.minY) / (bounds.maxY - bounds.minY || 1)) * plotH;

        // Grid lines & Axis labels
        ctx.font = `${this.options.fontSize}px ${this.options.fontFamily}`;
        ctx.fillStyle = th.textColor;
        ctx.lineWidth = 1;

        // Y Grid (5 divisions)
        const ySteps = 5;
        for (let i = 0; i <= ySteps; ++i) {
            const val = bounds.minY + (i / ySteps) * (bounds.maxY - bounds.minY);
            const cy = toY(val);

            ctx.strokeStyle = th.gridColor;
            ctx.beginPath();
            ctx.moveTo(pad.left, cy);
            ctx.lineTo(pad.left + plotW, cy);
            ctx.stroke();

            ctx.fillStyle = th.textColor;
            ctx.textAlign = 'right';
            ctx.textBaseline = 'middle';
            let label = '';
            if (Math.abs(val) < 1e-4 && val !== 0) {
                label = val.toExponential(2);
            } else if (Math.abs(val) > 100) {
                label = val.toFixed(0);
            } else if (Math.abs(val) > 1) {
                label = val.toFixed(2);
            } else {
                label = val.toFixed(4);
            }
            ctx.fillText(label, pad.left - 8, cy);
        }

        // X Grid (5 divisions)
        const xSteps = 5;
        for (let i = 0; i <= xSteps; ++i) {
            const val = bounds.minX + (i / xSteps) * (bounds.maxX - bounds.minX);
            const cx = toX(val);

            ctx.strokeStyle = th.gridColor;
            ctx.beginPath();
            ctx.moveTo(cx, pad.top);
            ctx.lineTo(cx, pad.top + plotH);
            ctx.stroke();

            ctx.fillStyle = th.textColor;
            ctx.textAlign = 'center';
            ctx.textBaseline = 'top';
            ctx.fillText(val > 10 ? val.toFixed(0) : val.toFixed(2), cx, pad.top + plotH + 8);
        }

        // Main Axes
        ctx.strokeStyle = th.axisColor;
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        ctx.moveTo(pad.left, pad.top);
        ctx.lineTo(pad.left, pad.top + plotH);
        ctx.lineTo(pad.left + plotW, pad.top + plotH);
        ctx.stroke();

        // Axis Titles
        ctx.font = `600 ${this.options.fontSize}px ${this.options.fontFamily}`;
        ctx.fillStyle = th.titleColor;
        ctx.textAlign = 'center';
        ctx.fillText(this.options.xLabel, pad.left + plotW / 2, this.height - 12);

        ctx.save();
        ctx.translate(14, pad.top + plotH / 2);
        ctx.rotate(-Math.PI / 2);
        ctx.textAlign = 'center';
        ctx.fillText(this.options.yLabel, 0, 0);
        ctx.restore();

        // Render Series
        this.series.forEach(s => {
            if (!s.data || s.data.length === 0) return;

            // Draw line
            if (s.type !== 'scatter') {
                ctx.strokeStyle = s.color || '#2563eb';
                ctx.lineWidth = s.lineWidth || 2;
                ctx.beginPath();
                s.data.forEach((pt, idx) => {
                    const cx = toX(pt.x);
                    const cy = toY(pt.y);
                    if (idx === 0) ctx.moveTo(cx, cy);
                    else ctx.lineTo(cx, cy);
                });
                ctx.stroke();
            }

            // Draw points / markers
            if (s.showPoints || s.type === 'scatter') {
                ctx.fillStyle = s.color || '#2563eb';
                s.data.forEach(pt => {
                    const cx = toX(pt.x);
                    const cy = toY(pt.y);
                    ctx.beginPath();
                    ctx.arc(cx, cy, s.pointRadius || 4, 0, Math.PI * 2);
                    ctx.fill();
                    ctx.strokeStyle = th.bg;
                    ctx.lineWidth = 1.5;
                    ctx.stroke();
                });
            }
        });

        // Legend
        if (this.options.showLegend && this.series.length > 0) {
            let legX = pad.left + 8;
            const legY = pad.top - 16;
            ctx.font = `500 ${this.options.fontSize}px ${this.options.fontFamily}`;

            this.series.forEach(s => {
                ctx.fillStyle = s.color || '#2563eb';
                ctx.fillRect(legX, legY - 8, 12, 10);
                ctx.strokeStyle = th.axisColor;
                ctx.lineWidth = 1;
                ctx.strokeRect(legX, legY - 8, 12, 10);

                ctx.fillStyle = th.titleColor;
                ctx.textAlign = 'left';
                ctx.textBaseline = 'middle';
                ctx.fillText(s.name, legX + 16, legY - 3);
                legX += ctx.measureText(s.name).width + 28;
            });
        }

        // Tooltip on Hover
        if (this.isHovering && this.hoverPoint) {
            const hp = this.hoverPoint;
            ctx.beginPath();
            ctx.arc(hp.cx, hp.cy, 6, 0, Math.PI * 2);
            ctx.fillStyle = hp.color;
            ctx.fill();
            ctx.strokeStyle = '#ffffff';
            ctx.lineWidth = 2;
            ctx.stroke();

            const text = `${hp.seriesName}: (${hp.x.toFixed(2)}, ${hp.y.toFixed(4)})`;
            ctx.font = `500 11px ${this.options.fontFamily}`;
            const textW = ctx.measureText(text).width;
            let tipX = hp.cx + 10;
            let tipY = hp.cy - 20;

            if (tipX + textW + 16 > this.width) tipX = hp.cx - textW - 20;
            if (tipY < 12) tipY = hp.cy + 12;

            ctx.fillStyle = th.tooltipBg;
            ctx.strokeStyle = th.tooltipBorder;
            ctx.lineWidth = 1;
            ctx.fillRect(tipX, tipY - 14, textW + 12, 22);
            ctx.strokeRect(tipX, tipY - 14, textW + 12, 22);

            ctx.fillStyle = th.tooltipText;
            ctx.textAlign = 'left';
            ctx.textBaseline = 'middle';
            ctx.fillText(text, tipX + 6, tipY - 3);
        }
    }

    renderEmptyState(ctx, pad, plotW, plotH, th) {
        // Subtle outline of the plot box
        ctx.strokeStyle = th.gridColor;
        ctx.lineWidth = 1;
        ctx.strokeRect(pad.left, pad.top, plotW, plotH);

        // Axis Titles
        ctx.font = `600 ${this.options.fontSize}px ${this.options.fontFamily}`;
        ctx.fillStyle = th.textColor;
        ctx.textAlign = 'center';
        ctx.fillText(this.options.xLabel, pad.left + plotW / 2, this.height - 12);

        ctx.save();
        ctx.translate(14, pad.top + plotH / 2);
        ctx.rotate(-Math.PI / 2);
        ctx.textAlign = 'center';
        ctx.fillText(this.options.yLabel, 0, 0);
        ctx.restore();

        // Informative empty message
        ctx.font = `500 13px ${this.options.fontFamily}`;
        ctx.fillStyle = th.emptyColor;
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(this.options.emptyMessage || 'No data to display', pad.left + plotW / 2, pad.top + plotH / 2);
    }

    renderBarChart(ctx, pad, plotW, plotH, th) {
        if (!this.series || this.series.length === 0) return;

        const data = this.series[0].data || [];
        if (data.length === 0) return;

        let maxVal = Math.max(...data.map(d => Math.max(d.speedup || 0, d.ideal || 0, 1)));
        maxVal = Math.ceil(maxVal * 1.15);

        const toY = (v) => pad.top + plotH - (v / maxVal) * plotH;

        // Y Grid
        const ySteps = 4;
        ctx.font = `${this.options.fontSize}px ${this.options.fontFamily}`;
        ctx.fillStyle = th.textColor;
        for (let i = 0; i <= ySteps; ++i) {
            const val = (i / ySteps) * maxVal;
            const cy = toY(val);

            ctx.strokeStyle = th.gridColor;
            ctx.beginPath();
            ctx.moveTo(pad.left, cy);
            ctx.lineTo(pad.left + plotW, cy);
            ctx.stroke();

            ctx.textAlign = 'right';
            ctx.textBaseline = 'middle';
            ctx.fillText(val.toFixed(1) + 'x', pad.left - 8, cy);
        }

        // Axes
        ctx.strokeStyle = th.axisColor;
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        ctx.moveTo(pad.left, pad.top);
        ctx.lineTo(pad.left, pad.top + plotH);
        ctx.lineTo(pad.left + plotW, pad.top + plotH);
        ctx.stroke();

        const slotW = plotW / data.length;
        const barW = Math.min(36, slotW * 0.35);

        data.forEach((d, idx) => {
            const slotCenterX = pad.left + idx * slotW + slotW / 2;

            // Measured Bar
            const measuredH = (d.speedup / maxVal) * plotH;
            const measuredX = slotCenterX - barW - 2;
            const measuredY = pad.top + plotH - measuredH;

            ctx.fillStyle = '#2563eb'; // Royal blue accent
            ctx.fillRect(measuredX, measuredY, barW, measuredH);

            // Measured value on top
            ctx.fillStyle = th.barValueColor;
            ctx.textAlign = 'center';
            ctx.textBaseline = 'bottom';
            ctx.fillText(d.speedup.toFixed(2) + 'x', measuredX + barW / 2, measuredY - 4);

            // Ideal Linear Bar
            const idealH = (d.ideal / maxVal) * plotH;
            const idealX = slotCenterX + 2;
            const idealY = pad.top + plotH - idealH;

            ctx.fillStyle = th.idealBarColor;
            ctx.fillRect(idealX, idealY, barW, idealH);

            ctx.fillStyle = th.idealTextColor;
            ctx.fillText(d.ideal.toFixed(1) + 'x', idealX + barW / 2, idealY - 4);

            // X Label
            ctx.fillStyle = th.titleColor;
            ctx.textBaseline = 'top';
            ctx.fillText(d.label || `${d.workers} Core`, slotCenterX, pad.top + plotH + 8);
        });

        // Legend
        const legY = pad.top - 16;
        ctx.fillStyle = '#2563eb';
        ctx.fillRect(pad.left + 8, legY - 8, 12, 10);
        ctx.fillStyle = th.titleColor;
        ctx.textAlign = 'left';
        ctx.textBaseline = 'middle';
        ctx.fillText('Measured Speedup', pad.left + 24, legY - 3);

        ctx.fillStyle = th.idealBarColor;
        ctx.fillRect(pad.left + 160, legY - 8, 12, 10);
        ctx.fillStyle = th.titleColor;
        ctx.fillText('Ideal Linear (y = x)', pad.left + 176, legY - 3);
    }
}

window.ChartEngine = ChartEngine;
