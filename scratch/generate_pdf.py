import os
import re
import sys
import subprocess
import pypdf

REPO_ROOT = r"C:\Users\devha\OneDrive\Desktop\My Folder\UnifiedEngineeringWorkbench"
REPORT_MD_PATH = os.path.join(REPO_ROOT, "docs", "final_report.md")
OUTPUT_PDF_PATH = os.path.join(REPO_ROOT, "Unified_Engineering_Workbench_Final_Report.pdf")
TEMP_HTML_PATH = os.path.join(REPO_ROOT, "scratch", "final_report_print.html")
EDGE_PATH = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"

def render_math_to_html(tex, display=False):
    s = tex.strip()
    
    # 1. Handle matrices: \begin{bmatrix} ... \end{bmatrix}
    def format_matrix(match):
        mat_content = match.group(1).strip()
        rows = mat_content.split(r'\\')
        mat_html = ['<span class="mat-bracket">[</span><span class="mat-grid">']
        for row in rows:
            cols = row.split('&')
            mat_html.append('<span class="mat-row">' + ''.join(f'<span class="mat-cell">{render_math_to_html(c.strip(), display=False)}</span>' for c in cols) + '</span>')
        mat_html.append('</span><span class="mat-bracket">]</span>')
        return "".join(mat_html)

    s = re.sub(r'\\begin\{bmatrix\}(.*?)\\end\{bmatrix\}', format_matrix, s, flags=re.DOTALL)
    
    # 2. Fractions \frac{num}{den}
    while r'\frac' in s:
        s_new = re.sub(r'\\frac\{([^{}]+)\}\{([^{}]+)\}', r'<span class="frac"><span class="num">\1</span><span class="den">\2</span></span>', s)
        if s_new == s:
            break
        s = s_new
        
    # 3. Clean up specific compound symbols & degree units
    s = re.sub(r'\^\{\\circ\\text\{C\}\}', '&deg;C', s)
    s = re.sub(r'\^\\circ\\text\{C\}', '&deg;C', s)
    s = re.sub(r'\\circ\\text\{C\}', '&deg;C', s)
    s = re.sub(r'\^\\circ', '&deg;', s)
    s = re.sub(r'\\circ', '&deg;', s)
    s = re.sub(r'\\mu', '&mu;', s)
    s = re.sub(r'\\Delta', '&Delta;', s)
    s = re.sub(r'\\theta', '&theta;', s)
    s = re.sub(r'\\eta', '&eta;', s)
    s = re.sub(r'\\delta', '&delta;', s)
    s = re.sub(r'\\approx', '&approx;', s)
    s = re.sub(r'\\le', '&le;', s)
    s = re.sub(r'\\ge', '&ge;', s)
    s = re.sub(r'\\ll', '&#8810;', s) # double less than ≪
    s = re.sub(r'\\times', '&times;', s)
    s = re.sub(r'\\cdot', '&middot;', s)
    s = re.sub(r'\\in', '&isin;', s)
    s = re.sub(r'\\rightarrow', '&rarr;', s)
    s = re.sub(r'\\nabla', '&nabla;', s)
    s = re.sub(r'\\partial', '&part;', s)
    s = re.sub(r'\\epsilon', '&epsilon;', s)
    s = re.sub(r'\\quad', '&emsp;', s)
    s = re.sub(r'\\vert', '|', s)
    s = re.sub(r'\\\|', '&Vert;', s)
    s = re.sub(r'\\\{', '{', s)
    s = re.sub(r'\\\}', '}', s)
    
    # Text commands and font styling
    s = re.sub(r'\\mathbf\{([^\}]+)\}', r'<strong>\1</strong>', s)
    s = re.sub(r'_\{\\text\{([^\}]+)\}\}', r'<sub>\1</sub>', s)
    s = re.sub(r'\\text\{([^\}]+)\}', r'<span class="m-text">\1</span>', s)
    s = re.sub(r'_\{\\min\}', '<sub>min</sub>', s)
    s = re.sub(r'_\{\\max\}', '<sub>max</sub>', s)
    s = re.sub(r'\\min', 'min', s)
    s = re.sub(r'\\max', 'max', s)
    s = re.sub(r'\\tanh', '<span class="m-text">tanh</span>', s)
    
    # Subscripts: _{...} and _x
    s = re.sub(r'_\{([^\}]+)\}', r'<sub>\1</sub>', s)
    s = re.sub(r'_([0-9a-zA-Z]+)', r'<sub>\1</sub>', s)
    
    # Superscripts: ^{...} and ^x
    s = re.sub(r'\^\{([^\}]+)\}', r'<sup>\1</sup>', s)
    s = re.sub(r'\^([0-9a-zA-Z\+\-]+)', r'<sup>\1</sup>', s)
    
    # Render minus inside superscripts cleanly as true minus
    s = re.sub(r'<sup>([+-]?\d+)</sup>', lambda m: '<sup>' + m.group(1).replace('-', '&minus;') + '</sup>', s)
    s = s.replace('<sup>-', '<sup>&minus;')
    
    # Clean up single backslashes in front of spaces
    s = s.replace(r'\,', '&thinsp;').replace(r'\ ', ' ')
    
    if display:
        return f'<div class="math-block">{s}</div>'
    else:
        return f'<span class="math-span">{s}</span>'

def format_inline_markdown(text):
    # Protect math blocks first
    math_placeholders = []
    def save_math(m):
        math_placeholders.append(m.group(0))
        return f"__MATH_TAG_{len(math_placeholders)-1}__"
    
    # Display math $$ ... $$
    text = re.sub(r'\$\$([^\$]+)\$\$', save_math, text)
    # Inline math $ ... $
    text = re.sub(r'\$([^\$]+)\$', save_math, text)
    
    # Bold + Italic
    text = re.sub(r'\*\*\*([^\*]+)\*\*\*', r'<strong><em>\1</em></strong>', text)
    # Bold
    text = re.sub(r'\*\*([^\*]+)\*\*', r'<strong>\1</strong>', text)
    # Italic
    text = re.sub(r'\*([^\*]+)\*', r'<em>\1</em>', text)
    # Inline code
    text = re.sub(r'`([^`]+)`', r'<code>\1</code>', text)
    
    # Restore math
    for i, mb in enumerate(math_placeholders):
        if mb.startswith('$$'):
            rendered = render_math_to_html(mb[2:-2], display=True)
            text = text.replace(f"__MATH_TAG_{i}__", rendered)
        else:
            rendered = render_math_to_html(mb[1:-1], display=False)
            text = text.replace(f"__MATH_TAG_{i}__", rendered)
            
    return text

def render_architecture_diagram():
    return """
<div class="arch-diagram">
    <div class="arch-layer ui-layer">
        <div class="layer-header">
            <span class="layer-title">LAYER 1: USER INTERFACE</span>
            <span class="layer-subtitle">Air-Gapped HTML5 &bull; Modern CSS3 &bull; Native ES6+ JavaScript</span>
        </div>
        <div class="layer-boxes">
            <div class="arch-box">
                <strong>3D WebGL Orbit Viewport</strong><br>
                <code>canvas_3d.js</code> &bull; Native Shaders &bull; 2D Fallback
            </div>
            <div class="arch-box">
                <strong>High-DPI Chart Engine</strong><br>
                <code>chart_engine.js</code> &bull; Analytical &amp; FEM Curves &bull; Log Loss
            </div>
            <div class="arch-box">
                <strong>Design System &amp; Responsive Layout</strong><br>
                <code>tokens.css</code> &bull; <code>responsive.css</code> &bull; 5 Breakpoints
            </div>
        </div>
    </div>
    
    <div class="arch-connector">
        <div class="connector-arrow">&darr;&uarr;</div>
        <div class="connector-label">
            <strong>Asynchronous JSON IPC Bridge (<code>HostBridge</code>)</strong><br>
            Strict Action Allowlist &bull; 64 KB Payload Bounds &bull; Native Win32 File Dialogs &bull; No Direct Path Access
        </div>
    </div>
    
    <div class="arch-layer host-layer">
        <div class="layer-header">
            <span class="layer-title">LAYER 2: WIN32 DESKTOP APPLICATION HOST</span>
            <span class="layer-subtitle">workbench_desktop.exe / Launch.exe &bull; Win32 API &bull; Microsoft Edge WebView2</span>
        </div>
        <div class="layer-boxes">
            <div class="arch-box">
                <strong>Native Win32 Common Dialogs</strong><br>
                <code>GetOpenFileNameW</code> &bull; <code>GetSaveFileNameW</code>
            </div>
            <div class="arch-box">
                <strong>Thread-Safe JSON Message Dispatcher</strong><br>
                Origin Check &bull; Action Whitelist &bull; Async Workers
            </div>
            <div class="arch-box">
                <strong>Evergreen Runtime Initialization</strong><br>
                Preflight Check &bull; Graceful Dialog Fallbacks
            </div>
        </div>
    </div>
    
    <div class="arch-connector">
        <div class="connector-arrow">&darr;&uarr;</div>
        <div class="connector-label">
            <strong>Direct In-Process C++17 Function Calls &amp; Type-Safe Interfaces</strong>
        </div>
    </div>
    
    <div class="arch-layer core-layer">
        <div class="layer-header">
            <span class="layer-title">LAYER 3: NATIVE C++17 ENGINEERING CORE</span>
            <span class="layer-subtitle">High-Performance Numerical Solvers &bull; Multi-Core Benchmarking &bull; Hardware Abstraction</span>
        </div>
        <div class="layer-grid-4">
            <div class="arch-box"><strong>Geometry Engine</strong><br>ASCII STL &bull; CSV Temp Map</div>
            <div class="arch-box"><strong>Thermal FEM Solver</strong><br>TDMA <em>O(N)</em> &bull; Convergence</div>
            <div class="arch-box"><strong>Benchmark Pool</strong><br>Dynamic Work-Stealing</div>
            <div class="arch-box"><strong>Hardware Registry</strong><br>Simulated MCU/FPGA/JTAG</div>
            <div class="arch-box"><strong>Backpropagation</strong><br>Exact Chain Rule &bull; &Delta; &le; 10<sup>&minus;10</sup></div>
            <div class="arch-box"><strong>System Metadata</strong><br>CPUID &bull; Core Topology &bull; OS</div>
            <div class="arch-box"><strong>Math Utilities</strong><br>Vector3D &bull; BoundingBox3D</div>
            <div class="arch-box"><strong>Thomas Algorithm</strong><br>Zero Linear Algebra Libs</div>
        </div>
    </div>
</div>
"""

def parse_markdown_to_html(md_text):
    lines = md_text.splitlines()
    html_out = []
    
    in_code_block = False
    code_block_lines = []
    
    in_table = False
    table_rows = []
    
    in_list = False
    list_type = None
    
    def close_list():
        nonlocal in_list, list_type
        if in_list:
            html_out.append(f"</{list_type}>")
            in_list = False
            list_type = None

    def close_table():
        nonlocal in_table, table_rows
        if in_table:
            html_out.append('<div class="table-wrap"><table>')
            if table_rows:
                header_cols = table_rows[0]
                html_out.append("<thead><tr>")
                for c in header_cols:
                    html_out.append(f"<th>{format_inline_markdown(c.strip())}</th>")
                html_out.append("</tr></thead>")
                
                start_idx = 1
                if len(table_rows) > 1 and all(re.match(r'^:?-+:?$', c.strip()) for c in table_rows[1]):
                    start_idx = 2
                
                html_out.append("<tbody>")
                for r in table_rows[start_idx:]:
                    html_out.append("<tr>")
                    for idx, c in enumerate(r):
                        val = format_inline_markdown(c.strip())
                        if "PASSED" in val:
                            val = val.replace("PASSED", '<span class="badge badge-pass">PASSED</span>')
                        elif "Reference" in val:
                            val = val.replace("Reference", '<span class="badge badge-ref">Reference</span>')
                        html_out.append(f"<td>{val}</td>")
                    html_out.append("</tr>")
                html_out.append("</tbody>")
            html_out.append("</table></div>")
            in_table = False
            table_rows = []

    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        
        # Check code block
        if stripped.startswith("```"):
            if in_code_block:
                in_code_block = False
                full_code = "\n".join(code_block_lines)
                if "USER INTERFACE" in full_code and "NATIVE C++17 CORE" in full_code:
                    html_out.append(render_architecture_diagram())
                else:
                    html_out.append("<pre class=\"code-box\"><code>" + full_code + "</code></pre>")
                code_block_lines = []
            else:
                close_list()
                close_table()
                in_code_block = True
                code_block_lines = []
            i += 1
            continue
            
        if in_code_block:
            esc = line.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
            code_block_lines.append(esc)
            i += 1
            continue
            
        # Empty lines
        if not stripped:
            close_list()
            close_table()
            i += 1
            continue
            
        # Horizontal rules
        if stripped in ["---", "***", "___"]:
            close_list()
            close_table()
            html_out.append('<hr class="section-divider" />')
            i += 1
            continue
            
        # Table lines
        if stripped.startswith("|") and stripped.endswith("|"):
            close_list()
            protected_line = re.sub(r'\$([^\$]+)\$', lambda m: '$' + m.group(1).replace('|', r'\vert') + '$', stripped)
            cols = [c for c in protected_line.split("|")[1:-1]]
            if not in_table:
                in_table = True
                table_rows = []
            table_rows.append(cols)
            i += 1
            continue
        else:
            close_table()
            
        # Headings
        if stripped.startswith("#"):
            close_list()
            match = re.match(r'^(#{1,6})\s+(.*)$', stripped)
            if match:
                level = len(match.group(1))
                h_text = format_inline_markdown(match.group(2))
                if level == 1:
                    html_out.append(f'<h1 class="report-title">{h_text}</h1>')
                elif level == 2:
                    html_out.append(f'<h2 class="h2-heading">{h_text}</h2>')
                elif level == 3:
                    html_out.append(f'<h3 class="h3-heading">{h_text}</h3>')
                else:
                    html_out.append(f'<h4 class="h4-heading">{h_text}</h4>')
                i += 1
                continue
                
        # Unordered list
        ul_match = re.match(r'^(\s*)[-\*]\s+(.*)$', line)
        if ul_match:
            if not in_list or list_type != 'ul':
                close_list()
                in_list = True
                list_type = 'ul'
                html_out.append('<ul class="report-ul">')
            item_text = format_inline_markdown(ul_match.group(2).strip())
            html_out.append(f'<li>{item_text}</li>')
            i += 1
            continue
            
        # Ordered list
        ol_match = re.match(r'^\s*(\d+)\.\s+(.*)$', line)
        if ol_match:
            if not in_list or list_type != 'ol':
                close_list()
                in_list = True
                list_type = 'ol'
                html_out.append('<ol class="report-ol">')
            item_text = format_inline_markdown(ol_match.group(2).strip())
            html_out.append(f'<li>{item_text}</li>')
            i += 1
            continue
            
        close_list()
        
        # Display math on its own line: $$ ... $$
        if stripped.startswith("$$") and stripped.endswith("$$") and len(stripped) > 4:
            tex_content = stripped[2:-2].strip()
            html_out.append(render_math_to_html(tex_content, display=True))
            i += 1
            continue
            
        # Format paragraph
        p_text = format_inline_markdown(stripped)
        html_out.append(f'<p class="report-p">{p_text}</p>')
        i += 1
        
    close_list()
    close_table()
    return "\n".join(html_out)

CSS_STYLING = """
@page {
    size: A4 portrait;
    margin: 14mm 16mm 14mm 16mm;
    @top-left {
        content: "Unified Engineering Workbench — Final Technical Report";
        font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
        font-size: 7.5pt;
        color: #718096;
        border-bottom: 0.5px solid #e2e8f0;
        padding-bottom: 2px;
    }
    @top-right {
        content: "PASS Science Works R&D Internship 2026–2027";
        font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
        font-size: 7.5pt;
        color: #718096;
        border-bottom: 0.5px solid #e2e8f0;
        padding-bottom: 2px;
    }
    @bottom-left {
        content: "Unified Engineering Workbench — Final Technical Report | Page " counter(page) " of " counter(pages);
        font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
        font-size: 7.5pt;
        color: #4a5568;
        border-top: 0.5px solid #e2e8f0;
        padding-top: 3px;
    }
    @bottom-right {
        content: "";
        border-top: 0.5px solid #e2e8f0;
        padding-top: 3px;
    }
}

* {
    box-sizing: border-box;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
    font-size: 8.8pt;
    line-height: 1.38;
    color: #2d3748;
    background-color: #ffffff;
    margin: 0;
    padding: 0;
    text-align: left;
}

.report-title {
    font-size: 16pt;
    font-weight: 700;
    color: #1a365d;
    margin: 0 0 6px 0;
    letter-spacing: -0.02em;
    border-bottom: 2px solid #2b6cb0;
    padding-bottom: 6px;
}

.h2-heading {
    font-size: 11.5pt;
    font-weight: 700;
    color: #2b6cb0;
    margin: 14px 0 6px 0;
    padding-bottom: 3px;
    border-bottom: 1px solid #e2e8f0;
    page-break-after: avoid;
    break-after: avoid;
}

.h3-heading {
    font-size: 9.8pt;
    font-weight: 600;
    color: #1a202c;
    margin: 10px 0 4px 0;
    page-break-after: avoid;
    break-after: avoid;
}

.h4-heading {
    font-size: 9pt;
    font-weight: 600;
    color: #4a5568;
    margin: 8px 0 3px 0;
    page-break-after: avoid;
    break-after: avoid;
}

.report-p {
    margin: 0 0 6px 0;
    text-align: left;
}

.report-ul, .report-ol {
    margin: 3px 0 8px 18px;
    padding: 0;
}

.report-ul li, .report-ol li {
    margin-bottom: 3px;
    text-align: left;
}

.section-divider {
    border: 0;
    height: 1px;
    background: #e2e8f0;
    margin: 10px 0;
}

/* Architecture Diagram */
.arch-diagram {
    margin: 10px 0 12px 0;
    page-break-inside: avoid;
    break-inside: avoid;
    display: flex;
    flex-direction: column;
    gap: 4px;
}

.arch-layer {
    border: 1px solid #cbd5e0;
    border-radius: 4px;
    padding: 6px 8px;
    background-color: #f8fafc;
}

.ui-layer { border-left: 4px solid #3182ce; }
.host-layer { border-left: 4px solid #4a5568; background-color: #f1f5f9; }
.core-layer { border-left: 4px solid #2b6cb0; background-color: #edf2f7; }

.layer-header {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: 4px;
    border-bottom: 1px solid #e2e8f0;
    padding-bottom: 2px;
}

.layer-title {
    font-size: 7.8pt;
    font-weight: 700;
    color: #1a365d;
    letter-spacing: 0.04em;
}

.layer-subtitle {
    font-size: 6.8pt;
    color: #718096;
}

.layer-boxes {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 5px;
}

.layer-grid-4 {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 4px;
}

.arch-box {
    background: #ffffff;
    border: 1px solid #cbd5e0;
    border-radius: 3px;
    padding: 3px 6px;
    font-size: 6.8pt;
    line-height: 1.25;
    color: #2d3748;
}

.arch-box strong {
    color: #1a365d;
    font-size: 7pt;
}

.arch-connector {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    padding: 1px 0;
}

.connector-arrow {
    font-size: 9pt;
    color: #3182ce;
    font-weight: bold;
}

.connector-label {
    background: #ffffff;
    border: 1px dashed #a0aec0;
    border-radius: 3px;
    padding: 2px 10px;
    font-size: 6.8pt;
    color: #4a5568;
    text-align: center;
}

/* Tables */
.table-wrap {
    margin: 6px 0 10px 0;
    page-break-inside: avoid;
    break-inside: avoid;
}

table {
    width: 100%;
    border-collapse: collapse;
    font-size: 7.8pt;
    line-height: 1.28;
}

th, td {
    border: 1px solid #cbd5e0;
    padding: 4px 6px;
    text-align: left;
}

th {
    background-color: #f1f5f9;
    color: #1e293b;
    font-weight: 600;
    border-bottom: 1.5px solid #94a3b8;
}

tr:nth-child(even) td {
    background-color: #f8fafc;
}

.badge {
    display: inline-block;
    padding: 1px 5px;
    border-radius: 2px;
    font-size: 6.8pt;
    font-weight: 600;
    text-transform: uppercase;
}

.badge-pass {
    background-color: #dcfce7;
    color: #166534;
    border: 1px solid #86efac;
}

.badge-ref {
    background-color: #f1f5f9;
    color: #475569;
    border: 1px solid #cbd5e0;
}

/* Code and Math Typography */
code {
    font-family: "Cascadia Code", Consolas, monospace;
    font-size: 7.8pt;
    background-color: #f1f5f9;
    color: #475569;
    padding: 1px 3px;
    border-radius: 2px;
    border: 1px solid #e2e8f0;
}

.math-block {
    text-align: center;
    margin: 6px 0;
    padding: 4px 0;
    font-family: "Cambria Math", "Times New Roman", serif;
    font-size: 9.2pt;
    color: #1a202c;
    page-break-inside: avoid;
    break-inside: avoid;
}

.math-span {
    font-family: "Cambria Math", "Times New Roman", serif;
    font-size: 9.2pt;
    letter-spacing: -0.01em;
}

sup {
    font-size: 70%;
    line-height: 0;
    position: relative;
    vertical-align: baseline;
    top: -0.44em;
    margin-left: 0.5px;
}

sub {
    font-size: 70%;
    line-height: 0;
    position: relative;
    vertical-align: baseline;
    bottom: -0.22em;
    margin-left: 0.5px;
}

.m-text {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    font-size: 8.5pt;
}

.frac {
    display: inline-flex;
    flex-direction: column;
    vertical-align: middle;
    text-align: center;
    padding: 0 2px;
    font-size: 80%;
}

.num {
    border-bottom: 1px solid #4a5568;
    padding-bottom: 1px;
}

.den {
    padding-top: 1px;
}

.mat-bracket {
    font-size: 130%;
    font-weight: 300;
    vertical-align: middle;
    margin: 0 1px;
}

.mat-grid {
    display: inline-flex;
    flex-direction: column;
    vertical-align: middle;
    padding: 0 2px;
}

.mat-row {
    display: flex;
    justify-content: space-around;
    gap: 8px;
}

.mat-cell {
    min-width: 12px;
    text-align: center;
}
"""

def generate_pdf():
    print(f"Reading markdown from: {REPORT_MD_PATH}")
    with open(REPORT_MD_PATH, "r", encoding="utf-8") as f:
        md_text = f.read()

    html_body = parse_markdown_to_html(md_text)

    full_html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Unified Engineering Workbench — Final Technical Report</title>
<style>
{CSS_STYLING}
</style>
</head>
<body>
{html_body}
</body>
</html>
"""

    os.makedirs(os.path.dirname(TEMP_HTML_PATH), exist_ok=True)
    with open(TEMP_HTML_PATH, "w", encoding="utf-8") as f:
        f.write(full_html)
    print(f"Wrote temporary HTML to: {TEMP_HTML_PATH}")

    # Invoke Edge headless
    cmd = [
        EDGE_PATH,
        "--headless",
        "--disable-gpu",
        "--run-all-compositor-stages-before-draw",
        f"--print-to-pdf={OUTPUT_PDF_PATH}",
        TEMP_HTML_PATH
    ]
    print("Executing Edge headless to generate polished PDF...")
    res = subprocess.run(cmd, capture_output=True, text=True)
    
    if not os.path.exists(OUTPUT_PDF_PATH):
        print(f"Error: PDF was not created! Stderr: {res.stderr}")
        return False

    file_size = os.path.getsize(OUTPUT_PDF_PATH)
    reader = pypdf.PdfReader(OUTPUT_PDF_PATH)
    page_count = len(reader.pages)
    print(f"Polished PDF Generated Successfully!")
    print(f"Path: {OUTPUT_PDF_PATH}")
    print(f"Size: {file_size} bytes ({file_size/1024:.1f} KB)")
    print(f"Page Count: {page_count}")
    return page_count

if __name__ == "__main__":
    generate_pdf()
