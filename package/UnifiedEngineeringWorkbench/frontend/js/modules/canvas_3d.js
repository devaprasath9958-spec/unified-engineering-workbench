/**
 * UNIFIED ENGINEERING WORKBENCH - 3D CANVAS & MESH ENGINE
 * Pure WebGL / HTML5 Canvas 3D Viewer (Zero external dependencies).
 * Renders STL triangular meshes, wireframes, and ANSYS temperature point clouds
 * with interactive orbit, pan, zoom, and colormapping.
 */

class Canvas3D {
    constructor(canvas, options = {}) {
        this.canvas = canvas;
        this.options = Object.assign({
            mode: 'shaded', // 'shaded', 'wireframe', 'points'
            backgroundColor: '#0f172a', // Charcoal / dark viewport
            wireframeColor: '#38bdf8',
            pointSize: 8.0,
            showAxes: true
        }, options);

        this.mesh = null;
        this.temperatureNodes = null;

        // Camera & Transformation state
        this.camera = {
            rotX: 30 * (Math.PI / 180),
            rotY: -45 * (Math.PI / 180),
            zoom: 1.0,
            panX: 0,
            panY: 0,
            center: [0, 0, 0],
            scale: 1.0
        };

        this.isDragging = false;
        this.dragButton = 0;
        this.lastMouse = { x: 0, y: 0 };

        this.initGL();
        this.initEvents();
        this.resize();
    }

    initGL() {
        this.gl = this.canvas.getContext('webgl', { antialias: true }) ||
                  this.canvas.getContext('experimental-webgl');
        if (!this.gl) {
            console.warn('[Canvas3D] WebGL not supported, falling back to 2D projective renderer.');
            this.use2D = true;
            this.ctx = this.canvas.getContext('2d');
            return;
        }

        const gl = this.gl;
        gl.enable(gl.DEPTH_TEST);
        gl.depthFunc(gl.LEQUAL);

        // Vertex Shader
        const vsSource = `
            attribute vec3 aPosition;
            attribute vec3 aNormal;
            attribute vec3 aColor;

            uniform mat4 uModelView;
            uniform mat4 uProjection;
            uniform vec3 uLightDir;

            varying vec3 vColor;
            varying float vLighting;

            void main() {
                gl_Position = uProjection * uModelView * vec4(aPosition, 1.0);
                gl_PointSize = 7.0;

                vec3 norm = normalize(aNormal);
                float diff = max(dot(norm, normalize(uLightDir)), 0.0);
                vLighting = 0.35 + 0.65 * diff; // Ambient + Diffuse
                vColor = aColor;
            }
        `;

        // Fragment Shader
        const fsSource = `
            precision mediump float;
            varying vec3 vColor;
            varying float vLighting;

            void main() {
                gl_FragColor = vec4(vColor * vLighting, 1.0);
            }
        `;

        this.program = this.createProgram(gl, vsSource, fsSource);
        if (this.program) {
            gl.useProgram(this.program);
            this.attribs = {
                position: gl.getAttribLocation(this.program, 'aPosition'),
                normal: gl.getAttribLocation(this.program, 'aNormal'),
                color: gl.getAttribLocation(this.program, 'aColor')
            };
            this.uniforms = {
                modelView: gl.getUniformLocation(this.program, 'uModelView'),
                projection: gl.getUniformLocation(this.program, 'uProjection'),
                lightDir: gl.getUniformLocation(this.program, 'uLightDir')
            };

            // Dedicated buffers for STL mesh
            this.vertexBuffer = gl.createBuffer();
            this.normalBuffer = gl.createBuffer();
            this.colorBuffer = gl.createBuffer();

            // Independent dedicated buffers for Temperature Point Cloud
            this.pointVertexBuffer = gl.createBuffer();
            this.pointNormalBuffer = gl.createBuffer();
            this.pointColorBuffer = gl.createBuffer();
        }
    }

    createShader(gl, type, source) {
        const shader = gl.createShader(type);
        gl.shaderSource(shader, source);
        gl.compileShader(shader);
        if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
            console.error('[Canvas3D] Shader compile error:', gl.getShaderInfoLog(shader));
            gl.deleteShader(shader);
            return null;
        }
        return shader;
    }

    createProgram(gl, vsSrc, fsSrc) {
        const vs = this.createShader(gl, gl.VERTEX_SHADER, vsSrc);
        const fs = this.createShader(gl, gl.FRAGMENT_SHADER, fsSrc);
        if (!vs || !fs) return null;

        const prog = gl.createProgram();
        gl.attachShader(prog, vs);
        gl.attachShader(prog, fs);
        gl.linkProgram(prog);
        if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
            console.error('[Canvas3D] Program link error:', gl.getProgramInfoLog(prog));
            return null;
        }
        return prog;
    }

    initEvents() {
        this.canvas.addEventListener('mousedown', (e) => {
            this.isDragging = true;
            this.dragButton = e.button;
            this.lastMouse = { x: e.clientX, y: e.clientY };
        });

        window.addEventListener('mousemove', (e) => {
            if (!this.isDragging) return;
            const dx = e.clientX - this.lastMouse.x;
            const dy = e.clientY - this.lastMouse.y;
            this.lastMouse = { x: e.clientX, y: e.clientY };

            if (this.dragButton === 0) { // Orbit
                this.camera.rotY += dx * 0.01;
                this.camera.rotX += dy * 0.01;
                this.camera.rotX = Math.max(-Math.PI / 2 + 0.01, Math.min(Math.PI / 2 - 0.01, this.camera.rotX));
            } else if (this.dragButton === 2) { // Pan
                this.camera.panX += dx * 0.003 / this.camera.zoom;
                this.camera.panY -= dy * 0.003 / this.camera.zoom;
            }
            this.render();
        });

        window.addEventListener('mouseup', () => {
            this.isDragging = false;
        });

        this.canvas.addEventListener('contextmenu', (e) => e.preventDefault());

        this.canvas.addEventListener('wheel', (e) => {
            e.preventDefault();
            const factor = e.deltaY < 0 ? 1.15 : 0.85;
            this.camera.zoom = Math.max(0.1, Math.min(20.0, this.camera.zoom * factor));
            this.render();
        }, { passive: false });

        // Touch support for mobile/tablet orbit
        this.canvas.addEventListener('touchstart', (e) => {
            if (e.touches.length === 1) {
                this.isDragging = true;
                this.dragButton = 0;
                this.lastMouse = { x: e.touches[0].clientX, y: e.touches[0].clientY };
            }
        }, { passive: true });

        window.addEventListener('touchmove', (e) => {
            if (!this.isDragging || e.touches.length !== 1) return;
            const dx = e.touches[0].clientX - this.lastMouse.x;
            const dy = e.touches[0].clientY - this.lastMouse.y;
            this.lastMouse = { x: e.touches[0].clientX, y: e.touches[0].clientY };

            this.camera.rotY += dx * 0.01;
            this.camera.rotX += dy * 0.01;
            this.camera.rotX = Math.max(-Math.PI / 2 + 0.01, Math.min(Math.PI / 2 - 0.01, this.camera.rotX));
            this.render();
        }, { passive: true });

        window.addEventListener('touchend', () => {
            this.isDragging = false;
        });

        window.addEventListener('resize', () => this.resize());

        if (window.ResizeObserver) {
            this.resizeObserver = new ResizeObserver(() => {
                this.resize();
            });
            this.resizeObserver.observe(this.canvas);
        }
    }

    resize() {
        const rect = this.canvas.getBoundingClientRect();
        let w = rect.width;
        let h = rect.height;

        if (!w || !h) {
            w = this.canvas.clientWidth || parseInt(this.canvas.style.width, 10) || 600;
            h = this.canvas.clientHeight || parseInt(this.canvas.style.height, 10) || 420;
        }
        if (!w || !h) return;

        const dpr = window.devicePixelRatio || 1;
        this.canvas.width = Math.round(w * dpr);
        this.canvas.height = Math.round(h * dpr);
        this.width = w;
        this.height = h;

        if (this.gl) {
            this.gl.viewport(0, 0, this.canvas.width, this.canvas.height);
        }
        this.render();
    }

    resetCamera() {
        this.camera.rotX = 30 * (Math.PI / 180);
        this.camera.rotY = -45 * (Math.PI / 180);
        this.camera.zoom = 1.0;
        this.camera.panX = 0;
        this.camera.panY = 0;
        this.render();
    }

    setMode(mode) {
        this.options.mode = mode;
        this.render();
    }

    setMesh(meshData) {
        this.mesh = meshData;
        if (meshData && meshData.bounding_box) {
            const bbox = meshData.bounding_box;
            const cx = (bbox.min[0] + bbox.max[0]) / 2;
            const cy = (bbox.min[1] + bbox.max[1]) / 2;
            const cz = (bbox.min[2] + bbox.max[2]) / 2;
            this.camera.center = [cx, cy, cz];

            const maxSpan = Math.max(bbox.span[0], bbox.span[1], bbox.span[2], 0.001);
            this.camera.scale = 1.5 / maxSpan;
        }

        if (this.gl && this.program && meshData && meshData.vertices) {
            const gl = this.gl;
            const verts = new Float32Array(meshData.vertices);

            // Adaptively expand normals if 1 normal per triangle (3 floats) instead of 1 per vertex (9 floats)
            let normals;
            if (meshData.normals && meshData.normals.length === meshData.rendered_triangles * 3) {
                normals = new Float32Array(meshData.rendered_triangles * 9);
                for (let i = 0; i < meshData.rendered_triangles; ++i) {
                    const nx = meshData.normals[i * 3];
                    const ny = meshData.normals[i * 3 + 1];
                    const nz = meshData.normals[i * 3 + 2];
                    for (let v = 0; v < 3; ++v) {
                        const idx = (i * 3 + v) * 3;
                        normals[idx] = nx;
                        normals[idx + 1] = ny;
                        normals[idx + 2] = nz;
                    }
                }
            } else if (meshData.normals) {
                normals = new Float32Array(meshData.normals);
            } else {
                normals = new Float32Array(verts.length);
            }

            // Shaded mesh colors (Slate blue/grey)
            const colors = new Float32Array(meshData.rendered_triangles * 3 * 3);
            for (let i = 0; i < meshData.rendered_triangles; ++i) {
                const r = 0.72, g = 0.78, b = 0.88;
                for (let v = 0; v < 3; ++v) {
                    const idx = (i * 3 + v) * 3;
                    colors[idx] = r;
                    colors[idx + 1] = g;
                    colors[idx + 2] = b;
                }
            }

            gl.bindBuffer(gl.ARRAY_BUFFER, this.vertexBuffer);
            gl.bufferData(gl.ARRAY_BUFFER, verts, gl.STATIC_DRAW);

            gl.bindBuffer(gl.ARRAY_BUFFER, this.normalBuffer);
            gl.bufferData(gl.ARRAY_BUFFER, normals, gl.STATIC_DRAW);

            gl.bindBuffer(gl.ARRAY_BUFFER, this.colorBuffer);
            gl.bufferData(gl.ARRAY_BUFFER, colors, gl.STATIC_DRAW);
        }

        this.render();
    }

    setTemperatureField(fieldData) {
        this.temperatureNodes = fieldData;
        if (fieldData && fieldData.bounding_box) {
            if (!this.mesh) {
                const bbox = fieldData.bounding_box;
                const cx = (bbox.min[0] + bbox.max[0]) / 2;
                const cy = (bbox.min[1] + bbox.max[1]) / 2;
                const cz = (bbox.min[2] + bbox.max[2]) / 2;
                this.camera.center = [cx, cy, cz];

                const maxSpan = Math.max(bbox.span[0], bbox.span[1], bbox.span[2], 0.001);
                this.camera.scale = 1.5 / maxSpan;
            }
        }

        if (this.gl && this.program && fieldData && fieldData.nodes) {
            const gl = this.gl;
            const nodes = fieldData.nodes;
            const minT = fieldData.min_temperature;
            const maxT = fieldData.max_temperature;
            const rangeT = maxT - minT || 1.0;

            const pts = new Float32Array(nodes.length * 3);
            const norms = new Float32Array(nodes.length * 3);
            const cols = new Float32Array(nodes.length * 3);

            nodes.forEach((n, idx) => {
                const i3 = idx * 3;
                pts[i3] = n.x;
                pts[i3 + 1] = n.y;
                pts[i3 + 2] = n.z;

                norms[i3] = 0;
                norms[i3 + 1] = 0;
                norms[i3 + 2] = 1;

                const normVal = (n.temperature_c - minT) / rangeT;
                const rgb = Canvas3D.temperatureToRGB(normVal);
                cols[i3] = rgb[0];
                cols[i3 + 1] = rgb[1];
                cols[i3 + 2] = rgb[2];
            });

            gl.bindBuffer(gl.ARRAY_BUFFER, this.pointVertexBuffer);
            gl.bufferData(gl.ARRAY_BUFFER, pts, gl.STATIC_DRAW);

            gl.bindBuffer(gl.ARRAY_BUFFER, this.pointNormalBuffer);
            gl.bufferData(gl.ARRAY_BUFFER, norms, gl.STATIC_DRAW);

            gl.bindBuffer(gl.ARRAY_BUFFER, this.pointColorBuffer);
            gl.bufferData(gl.ARRAY_BUFFER, cols, gl.STATIC_DRAW);
        }

        this.render();
    }

    static temperatureToRGB(val) {
        val = Math.max(0, Math.min(1, val));
        let r = 0, g = 0, b = 0;
        if (val < 0.25) {
            r = 0;
            g = val / 0.25;
            b = 1;
        } else if (val < 0.5) {
            r = 0;
            g = 1;
            b = 1 - (val - 0.25) / 0.25;
        } else if (val < 0.75) {
            r = (val - 0.5) / 0.25;
            g = 1;
            b = 0;
        } else {
            r = 1;
            g = 1 - (val - 0.75) / 0.25;
            b = 0;
        }
        return [r, g, b];
    }

    render() {
        if (!this.width || !this.height) return;

        if (this.use2D) {
            this.render2DFallback();
            return;
        }

        const gl = this.gl;
        gl.clearColor(0.06, 0.09, 0.16, 1.0); // #0f172a
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

        if (!this.mesh && !this.temperatureNodes) {
            return;
        }

        const aspect = this.width / this.height;
        const proj = this.createPerspectiveMatrix(45 * (Math.PI / 180), aspect, 0.1, 100.0);
        const mv = this.createModelViewMatrix();

        gl.useProgram(this.program);
        gl.uniformMatrix4fv(this.uniforms.projection, false, proj);
        gl.uniformMatrix4fv(this.uniforms.modelView, false, mv);
        gl.uniform3f(this.uniforms.lightDir, 0.6, 0.8, 1.0);

        // Draw STL Mesh
        if (this.mesh && this.mesh.rendered_triangles > 0 && this.vertexBuffer) {
            gl.bindBuffer(gl.ARRAY_BUFFER, this.vertexBuffer);
            gl.vertexAttribPointer(this.attribs.position, 3, gl.FLOAT, false, 0, 0);
            gl.enableVertexAttribArray(this.attribs.position);

            gl.bindBuffer(gl.ARRAY_BUFFER, this.normalBuffer);
            gl.vertexAttribPointer(this.attribs.normal, 3, gl.FLOAT, false, 0, 0);
            gl.enableVertexAttribArray(this.attribs.normal);

            gl.bindBuffer(gl.ARRAY_BUFFER, this.colorBuffer);
            gl.vertexAttribPointer(this.attribs.color, 3, gl.FLOAT, false, 0, 0);
            gl.enableVertexAttribArray(this.attribs.color);

            const count = this.mesh.rendered_triangles * 3;
            if (this.options.mode === 'wireframe') {
                gl.drawArrays(gl.LINE_LOOP, 0, count);
            } else if (this.options.mode === 'points') {
                gl.drawArrays(gl.POINTS, 0, count);
            } else {
                gl.drawArrays(gl.TRIANGLES, 0, count);
            }
        }

        // Draw Temperature Point Cloud (Independent dedicated buffers)
        if (this.temperatureNodes && this.temperatureNodes.nodes && this.pointVertexBuffer) {
            gl.bindBuffer(gl.ARRAY_BUFFER, this.pointVertexBuffer);
            gl.vertexAttribPointer(this.attribs.position, 3, gl.FLOAT, false, 0, 0);
            gl.enableVertexAttribArray(this.attribs.position);

            gl.bindBuffer(gl.ARRAY_BUFFER, this.pointNormalBuffer);
            gl.vertexAttribPointer(this.attribs.normal, 3, gl.FLOAT, false, 0, 0);
            gl.enableVertexAttribArray(this.attribs.normal);

            gl.bindBuffer(gl.ARRAY_BUFFER, this.pointColorBuffer);
            gl.vertexAttribPointer(this.attribs.color, 3, gl.FLOAT, false, 0, 0);
            gl.enableVertexAttribArray(this.attribs.color);

            gl.drawArrays(gl.POINTS, 0, this.temperatureNodes.nodes.length);
        }
    }

    createPerspectiveMatrix(fov, aspect, near, far) {
        const f = 1.0 / Math.tan(fov / 2);
        const rangeInv = 1.0 / (near - far);
        return new Float32Array([
            f / aspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, (near + far) * rangeInv, -1,
            0, 0, near * far * rangeInv * 2, 0
        ]);
    }

    createModelViewMatrix() {
        const m = new Float32Array(16);
        m[0] = 1; m[5] = 1; m[10] = 1; m[15] = 1;

        const dist = 3.5 / this.camera.zoom;
        const tx = this.camera.panX;
        const ty = this.camera.panY;
        const tz = -dist;

        const cosX = Math.cos(this.camera.rotX);
        const sinX = Math.sin(this.camera.rotX);
        const cosY = Math.cos(this.camera.rotY);
        const sinY = Math.sin(this.camera.rotY);

        const s = this.camera.scale;
        const cx = this.camera.center[0];
        const cy = this.camera.center[1];
        const cz = this.camera.center[2];

        // Rotation & Scale
        m[0] = cosY * s;
        m[1] = sinX * sinY * s;
        m[2] = -cosX * sinY * s;
        m[3] = 0;

        m[4] = 0;
        m[5] = cosX * s;
        m[6] = sinX * s;
        m[7] = 0;

        m[8] = sinY * s;
        m[9] = -sinX * cosY * s;
        m[10] = cosX * cosY * s;
        m[11] = 0;

        // Translation correctly incorporates model centering without division
        m[12] = -(cx * m[0] + cy * m[4] + cz * m[8]) + tx;
        m[13] = -(cx * m[1] + cy * m[5] + cz * m[9]) + ty;
        m[14] = -(cx * m[2] + cy * m[6] + cz * m[10]) + tz;
        m[15] = 1.0;

        return m;
    }

    render2DFallback() {
        const ctx = this.ctx;
        if (!ctx) return;

        ctx.fillStyle = this.options.backgroundColor || '#0f172a';
        ctx.fillRect(0, 0, this.width, this.height);

        ctx.fillStyle = '#64748b';
        ctx.font = '10px Segoe UI, sans-serif';
        ctx.textAlign = 'right';
        ctx.fillText('Canvas 2D Software Fallback', this.width - 12, this.height - 12);

        if (!this.mesh && !this.temperatureNodes) {
            ctx.fillStyle = '#94a3b8';
            ctx.font = '12px Segoe UI, sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText('3D Viewport Ready (Drag to Orbit, Wheel to Zoom)', this.width / 2, this.height / 2);
            return;
        }

        const m = this.createModelViewMatrix();
        const f = Math.min(this.width, this.height) * 0.75;
        const midX = this.width / 2;
        const midY = this.height / 2;

        const project = (x, y, z) => {
            const vx = m[0]*x + m[4]*y + m[8]*z + m[12];
            const vy = m[1]*x + m[5]*y + m[9]*z + m[13];
            const vz = m[2]*x + m[6]*y + m[10]*z + m[14];
            const dist = -vz || 1.0;
            const px = midX + (vx / dist) * f;
            const py = midY - (vy / dist) * f;
            return { x: px, y: py, z: vz };
        };

        // Render Mesh in 2D
        if (this.mesh && this.mesh.vertices && this.mesh.rendered_triangles > 0) {
            const verts = this.mesh.vertices;
            const triangles = this.mesh.rendered_triangles;

            for (let i = 0; i < triangles; ++i) {
                const idx = i * 9;
                const p1 = project(verts[idx], verts[idx+1], verts[idx+2]);
                const p2 = project(verts[idx+3], verts[idx+4], verts[idx+5]);
                const p3 = project(verts[idx+6], verts[idx+7], verts[idx+8]);

                const det = (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);

                if (this.options.mode === 'wireframe') {
                    ctx.strokeStyle = this.options.wireframeColor || '#38bdf8';
                    ctx.lineWidth = 1;
                    ctx.beginPath();
                    ctx.moveTo(p1.x, p1.y);
                    ctx.lineTo(p2.x, p2.y);
                    ctx.lineTo(p3.x, p3.y);
                    ctx.closePath();
                    ctx.stroke();
                } else if (this.options.mode === 'points') {
                    ctx.fillStyle = '#38bdf8';
                    [p1, p2, p3].forEach(p => {
                        ctx.beginPath();
                        ctx.arc(p.x, p.y, 2, 0, Math.PI * 2);
                        ctx.fill();
                    });
                } else {
                    const brightness = Math.max(0.3, Math.min(1.0, 0.4 + (det > 0 ? 0.4 : 0.1)));
                    const shade = Math.floor(180 * brightness);
                    ctx.fillStyle = `rgb(${shade}, ${shade + 10}, ${shade + 25})`;
                    ctx.strokeStyle = '#1e293b';
                    ctx.lineWidth = 0.5;
                    ctx.beginPath();
                    ctx.moveTo(p1.x, p1.y);
                    ctx.lineTo(p2.x, p2.y);
                    ctx.lineTo(p3.x, p3.y);
                    ctx.closePath();
                    ctx.fill();
                    ctx.stroke();
                }
            }
        }

        // Render Temperature Point Cloud in 2D
        if (this.temperatureNodes && this.temperatureNodes.nodes) {
            const nodes = this.temperatureNodes.nodes;
            const minT = this.temperatureNodes.min_temperature;
            const maxT = this.temperatureNodes.max_temperature;
            const rangeT = maxT - minT || 1.0;

            nodes.forEach(n => {
                const p = project(n.x, n.y, n.z);
                const normVal = (n.temperature_c - minT) / rangeT;
                const rgb = Canvas3D.temperatureToRGB(normVal);
                ctx.fillStyle = `rgb(${Math.round(rgb[0]*255)}, ${Math.round(rgb[1]*255)}, ${Math.round(rgb[2]*255)})`;
                ctx.beginPath();
                ctx.arc(p.x, p.y, 4, 0, Math.PI * 2);
                ctx.fill();
                ctx.strokeStyle = '#000000';
                ctx.lineWidth = 0.75;
                ctx.stroke();
            });
        }
    }
}

window.Canvas3D = Canvas3D;
