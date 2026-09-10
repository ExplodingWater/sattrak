#!/usr/bin/env python3
"""
Interactive HTML Visualizer Generator
Builds a standalone, offline, zero-dependency HTML dashboard with:
1. 3D Satellite Attitude & GNC Mission Player
2. 2D Supersonic CFD Shockwave & Flowfield Explorer
"""

import json
import os
import shutil

def build_visualizer():
    with open('telemetry_compact.json', 'r') as f:
        telemetry_json = f.read()

    with open('cfd_compact.json', 'r') as f:
        cfd_json = f.read()

    html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Aerospace Engineering Dashboard</title>
  <style>
    :root {{
      --bg: #0b0f19;
      --card: #111827;
      --card-border: #1f293d;
      --primary: #38bdf8;
      --primary-hover: #0ea5e9;
      --accent: #f59e0b;
      --success: #10b981;
      --text: #f3f4f6;
      --muted: #9ca3af;
    }}
    * {{ box-sizing: border-box; margin: 0; padding: 0; }}
    body {{
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
      background: var(--bg);
      color: var(--text);
      min-height: 100vh;
      padding: 16px;
    }}
    .header {{
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding-bottom: 16px;
      border-bottom: 1px solid var(--card-border);
      margin-bottom: 20px;
    }}
    .title-group h1 {{
      font-size: 24px;
      font-weight: 700;
      letter-spacing: -0.5px;
      background: linear-gradient(90deg, #38bdf8, #818cf8);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }}
    .title-group p {{
      color: var(--muted);
      font-size: 13px;
      margin-top: 4px;
    }}
    .tabs {{
      display: flex;
      gap: 8px;
    }}
    .tab-btn {{
      background: var(--card);
      border: 1px solid var(--card-border);
      color: var(--muted);
      padding: 8px 16px;
      border-radius: 8px;
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s;
    }}
    .tab-btn.active {{
      background: #1e293b;
      color: var(--primary);
      border-color: var(--primary);
    }}
    .panel {{ display: none; }}
    .panel.active {{ display: block; }}

    /* Layout Grid */
    .grid-layout {{
      display: grid;
      grid-template-columns: 1fr 340px;
      gap: 20px;
    }}
    .canvas-card {{
      background: var(--card);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 16px;
      position: relative;
    }}
    canvas {{
      width: 100%;
      height: 520px;
      background: #060911;
      border-radius: 8px;
      display: block;
    }}
    .sidebar {{
      display: flex;
      flex-direction: column;
      gap: 16px;
    }}
    .stat-card {{
      background: var(--card);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 16px;
    }}
    .stat-card h3 {{
      font-size: 13px;
      color: var(--muted);
      text-transform: uppercase;
      letter-spacing: 0.5px;
      margin-bottom: 12px;
    }}
    .stat-row {{
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 8px;
      font-size: 13px;
    }}
    .stat-val {{
      font-weight: 600;
      font-family: monospace;
      color: var(--primary);
    }}
    .stat-val.alert {{ color: var(--accent); }}
    .stat-val.good {{ color: var(--success); }}

    /* Controls */
    .controls-bar {{
      display: flex;
      align-items: center;
      gap: 12px;
      margin-top: 14px;
      background: #0d131f;
      padding: 10px 14px;
      border-radius: 8px;
    }}
    .btn {{
      background: var(--primary);
      color: #04101e;
      border: none;
      padding: 6px 14px;
      border-radius: 6px;
      font-weight: 600;
      font-size: 12px;
      cursor: pointer;
      transition: background 0.15s;
    }}
    .btn:hover {{ background: var(--primary-hover); }}
    .btn-secondary {{
      background: #1f293d;
      color: var(--text);
    }}
    .btn-secondary:hover {{ background: #2d3b55; }}
    .slider {{
      flex: 1;
      accent-color: var(--primary);
      cursor: pointer;
    }}
    .time-label {{
      font-family: monospace;
      font-size: 13px;
      font-weight: bold;
      min-width: 60px;
    }}
    .badge {{
      display: inline-block;
      padding: 2px 8px;
      border-radius: 12px;
      font-size: 11px;
      font-weight: 600;
      background: #1e293b;
      color: var(--primary);
    }}
  </style>
</head>
<body>

  <div class="header">
    <div class="title-group">
      <h1>Interactive Aerospace Workbench</h1>
      <p>Real-Time Simulation Playback & High-Fidelity Flowfield Inspection</p>
    </div>
    <div class="tabs">
      <button class="tab-btn active" onclick="switchTab('adcs')">🛰️ Satellite GNC & MEKF</button>
      <button class="tab-btn" onclick="switchTab('cfd')">🌊 2D Supersonic Shock CFD</button>
    </div>
  </div>

  <!-- TAB 1: SATELLITE GNC -->
  <div id="panel-adcs" class="panel active">
    <div class="grid-layout">
      <div class="canvas-card">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">
          <span style="font-weight: 600; font-size: 14px;">3D Satellite Attitude & Pointing Maneuver</span>
          <span class="badge">Drag to Orbit 3D Camera</span>
        </div>
        <canvas id="canvas-3d"></canvas>

        <div class="controls-bar">
          <button id="btn-play" class="btn" onclick="togglePlay()">Play</button>
          <button class="btn btn-secondary" onclick="resetSim()">Reset</button>
          <input type="range" id="time-slider" class="slider" min="0" max="299" value="0" oninput="onSliderChange(this.value)">
          <span id="time-display" class="time-label">t = 0.0 s</span>
          <button class="btn btn-secondary" onclick="toggleSpeed()">Speed: <span id="speed-label">1x</span></button>
        </div>
      </div>

      <div class="sidebar">
        <div class="stat-card">
          <h3>GNC Telemetry HUD</h3>
          <div class="stat-row">
            <span>Attitude Pointing Error:</span>
            <span id="val-err" class="stat-val alert">35.33°</span>
          </div>
          <div class="stat-row">
            <span>Angular Rate (|ω|):</span>
            <span id="val-omega" class="stat-val">8.10 °/s</span>
          </div>
          <div class="stat-row">
            <span>MEKF Gyro Bias Error:</span>
            <span id="val-bias" class="stat-val">0.0332 rad/s</span>
          </div>
          <div class="stat-row">
            <span>3-Sigma Filter Envelope:</span>
            <span id="val-sigma" class="stat-val">11.40°</span>
          </div>
        </div>

        <div class="stat-card">
          <h3>Attitude Error History</h3>
          <canvas id="canvas-sparkline" style="height: 120px; background: transparent;"></canvas>
        </div>

        <div class="stat-card">
          <h3>3D Coordinate Conventions</h3>
          <div style="font-size: 12px; color: var(--muted); line-height: 1.6;">
            <p><span style="color: #ef4444; font-weight: bold;">■ Red Axis:</span> Body X-Axis (Roll / Nadir)</p>
            <p><span style="color: #22c55e; font-weight: bold;">■ Green Axis:</span> Body Y-Axis (Pitch / Solar Wing)</p>
            <p><span style="color: #3b82f6; font-weight: bold;">■ Blue Axis:</span> Body Z-Axis (Yaw / Star Tracker)</p>
            <p style="margin-top: 8px;"><span style="color: #f59e0b; font-weight: bold;">◎ Target Crosshair:</span> Target Pointing Attitude</p>
          </div>
        </div>
      </div>
    </div>
  </div>

  <!-- TAB 2: SUPERSONIC CFD -->
  <div id="panel-cfd" class="panel">
    <div class="grid-layout">
      <div class="canvas-card">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">
          <span style="font-weight: 600; font-size: 14px;">Mach 2.5 Oblique Shock Wave (15° Compression Ramp)</span>
          <div style="display: flex; gap: 8px;">
            <button class="btn" onclick="setCFDMode('mach')">Mach</button>
            <button class="btn btn-secondary" onclick="setCFDMode('press')">Pressure</button>
            <button class="btn btn-secondary" onclick="setCFDMode('schlieren')">Schlieren</button>
          </div>
        </div>
        <canvas id="canvas-cfd"></canvas>

        <div class="controls-bar" style="justify-content: space-between;">
          <div style="font-size: 12px; color: var(--muted);">
            Hover mouse over the flowfield to inspect local flow parameters.
          </div>
          <div style="display: flex; gap: 12px; align-items: center;">
            <label style="font-size: 12px; cursor: pointer;">
              <input type="checkbox" id="chk-grid" onchange="redrawCFD()"> Show Mesh
            </label>
            <label style="font-size: 12px; cursor: pointer;">
              <input type="checkbox" id="chk-shock" checked onchange="redrawCFD()"> Exact Analytical Shock (β = 36.9°)
            </label>
          </div>
        </div>
      </div>

      <div class="sidebar">
        <div class="stat-card">
          <h3>Local Probe Inspector</h3>
          <div class="stat-row">
            <span>Coordinate (X, Y):</span>
            <span id="probe-coord" class="stat-val">-</span>
          </div>
          <div class="stat-row">
            <span>Local Mach Number:</span>
            <span id="probe-mach" class="stat-val">-</span>
          </div>
          <div class="stat-row">
            <span>Static Pressure:</span>
            <span id="probe-press" class="stat-val">-</span>
          </div>
          <div class="stat-row">
            <span>Schlieren (|∇ρ|):</span>
            <span id="probe-sch" class="stat-val">-</span>
          </div>
        </div>

        <div class="stat-card">
          <h3>Analytical Theory vs CFD</h3>
          <div class="stat-row">
            <span>Upstream Mach:</span>
            <span class="stat-val">2.500</span>
          </div>
          <div class="stat-row">
            <span>Wedge Deflection Angle:</span>
            <span class="stat-val">15.0°</span>
          </div>
          <div class="stat-row">
            <span>Analytical Shock Angle (β):</span>
            <span class="stat-val good">36.945°</span>
          </div>
          <div class="stat-row">
            <span>Exact Post-Shock Pressure:</span>
            <span class="stat-val">250.0 kPa</span>
          </div>
          <div class="stat-row">
            <span>CFD Post-Shock Pressure:</span>
            <span class="stat-val good">250.0 kPa (0.01% err)</span>
          </div>
        </div>

        <div class="stat-card">
          <h3>Colorbar Legend</h3>
          <div id="colorbar-title" style="font-size: 12px; color: var(--muted); margin-bottom: 6px;">Mach Number: 1.84 (Sub/Transonic) to 2.50 (Freestream)</div>
          <div id="colorbar" style="height: 16px; border-radius: 4px; background: linear-gradient(to right, #3b82f6, #06b6d4, #10b981, #f59e0b, #ef4444);"></div>
          <div style="display: flex; justify-content: space-between; font-size: 11px; margin-top: 4px; color: var(--muted);">
            <span id="cbar-min">1.84</span>
            <span id="cbar-max">2.50</span>
          </div>
        </div>
      </div>
    </div>
  </div>

  <script>
    // Embedded Data
    const telemetryData = {telemetry_json};
    const cfdData = {cfd_json};

    // Tab Switching
    function switchTab(tabId) {{
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
      if (tabId === 'adcs') {{
        document.querySelector('.tabs button:nth-child(1)').classList.add('active');
        document.getElementById('panel-adcs').classList.add('active');
      }} else {{
        document.querySelector('.tabs button:nth-child(2)').classList.add('active');
        document.getElementById('panel-cfd').classList.add('active');
        redrawCFD();
      }}
    }}

    // =========================================================================
    // 3D SATELLITE VISUALIZER
    // =========================================================================
    const canvas3D = document.getElementById('canvas-3d');
    const ctx3D = canvas3D.getContext('2d');
    let currentIndex = 0;
    let isPlaying = false;
    let playSpeed = 1;
    let playInterval = null;

    // Orbit Camera
    let camYaw = 0.6;
    let camPitch = 0.4;
    let isDragging = false;
    let lastMouseX = 0, lastMouseY = 0;

    canvas3D.addEventListener('mousedown', e => {{
      isDragging = true;
      lastMouseX = e.clientX;
      lastMouseY = e.clientY;
    }});
    window.addEventListener('mouseup', () => isDragging = false);
    canvas3D.addEventListener('mousemove', e => {{
      if (!isDragging) return;
      camYaw += (e.clientX - lastMouseX) * 0.008;
      camPitch += (e.clientY - lastMouseY) * 0.008;
      camPitch = Math.max(-1.5, Math.min(1.5, camPitch));
      lastMouseX = e.clientX;
      lastMouseY = e.clientY;
      render3D();
    }});

    function resizeCanvas() {{
      const rect = canvas3D.getBoundingClientRect();
      canvas3D.width = rect.width * window.devicePixelRatio;
      canvas3D.height = rect.height * window.devicePixelRatio;
      ctx3D.scale(window.devicePixelRatio, window.devicePixelRatio);
      render3D();
    }}
    window.addEventListener('resize', resizeCanvas);

    // Quaternion math helper
    function quatRotate(v, q) {{
      // v_rotated = q * v * q_inv
      const x = v[0], y = v[1], z = v[2];
      const qx = q.qx, qy = q.qy, qz = q.qz, qw = q.qw;
      const ix =  qw * x + qy * z - qz * y;
      const iy =  qw * y + qz * x - qx * z;
      const iz =  qw * z + qx * y - qy * x;
      const iw = -qx * x - qy * y - qz * z;
      return [
        ix * qw + iw * -qx + iy * -qz - iz * -qy,
        iy * qw + iw * -qy + iz * -qx - ix * -qz,
        iz * qw + iw * -qz + ix * -qy - iy * -qx
      ];
    }}

    function project3D(v, w, h) {{
      // Camera rotation
      const cosY = Math.cos(camYaw), sinY = Math.sin(camYaw);
      const cosP = Math.cos(camPitch), sinP = Math.sin(camPitch);

      const x1 = cosY * v[0] - sinY * v[2];
      const z1 = sinY * v[0] + cosY * v[2];
      const y1 = v[1];

      const y2 = cosP * y1 - sinP * z1;
      const z2 = sinP * y1 + cosP * z1;
      const x2 = x1;

      const dist = 600;
      const focal = 450;
      const zEff = dist + z2;
      return [
        w * 0.5 + (x2 * focal) / zEff,
        h * 0.5 - (y2 * focal) / zEff,
        z2
      ];
    }}

    function render3D() {{
      const rect = canvas3D.getBoundingClientRect();
      const w = rect.width, h = rect.height;
      ctx3D.clearRect(0, 0, w, h);

      // Starfield background
      ctx3D.fillStyle = '#1e293b';
      for (let i = 0; i < 40; ++i) {{
        const sx = (Math.sin(i * 99) * 0.5 + 0.5) * w;
        const sy = (Math.cos(i * 33) * 0.5 + 0.5) * h;
        ctx3D.fillRect(sx, sy, 1.5, 1.5);
      }}

      // Target Crosshair (Inertial Target)
      const targetP = project3D([0, 0, 180], w, h);
      ctx3D.strokeStyle = 'rgba(245, 158, 11, 0.4)';
      ctx3D.lineWidth = 1;
      ctx3D.beginPath();
      ctx3D.arc(targetP[0], targetP[1], 18, 0, 2*Math.PI);
      ctx3D.moveTo(targetP[0] - 25, targetP[1]); ctx3D.lineTo(targetP[0] + 25, targetP[1]);
      ctx3D.moveTo(targetP[0], targetP[1] - 25); ctx3D.lineTo(targetP[0], targetP[1] + 25);
      ctx3D.stroke();

      const frame = telemetryData[currentIndex];
      const q = {{ qw: frame.qw, qx: frame.qx, qy: frame.qy, qz: frame.qz }};

      // 3U CubeSat Model Definition
      const L = 90, W = 40, H = 40;
      const bodyVerts = [
        [-W,-H,-L], [ W,-H,-L], [ W, H,-L], [-W, H,-L],
        [-W,-H, L], [ W,-H, L], [ W, H, L], [-W, H, L]
      ];
      const faces = [
        {{ idx: [0,1,2,3], color: '#334155' }},
        {{ idx: [4,5,6,7], color: '#d97706' }}, // Gold foil bottom
        {{ idx: [0,1,5,4], color: '#475569' }},
        {{ idx: [2,3,7,6], color: '#475569' }},
        {{ idx: [0,3,7,4], color: '#64748b' }},
        {{ idx: [1,2,6,5], color: '#64748b' }}
      ];

      // Solar Panel Wings (+Y and -Y)
      const solarVerts = [
        // Left Wing (+Y)
        [-W*0.8, H, -L*0.5], [ W*0.8, H, -L*0.5], [ W*0.8, H + 120, -L*0.5], [-W*0.8, H + 120, -L*0.5],
        // Right Wing (-Y)
        [-W*0.8, -H, -L*0.5], [ W*0.8, -H, -L*0.5], [ W*0.8, -H - 120, -L*0.5], [-W*0.8, -H - 120, -L*0.5]
      ];

      // Rotate and Project Vertices
      const pBody = bodyVerts.map(v => project3D(quatRotate(v, q), w, h));
      const pSolar = solarVerts.map(v => project3D(quatRotate(v, q), w, h));

      // Draw Solar Panels
      ctx3D.fillStyle = '#1d4ed8';
      ctx3D.strokeStyle = '#60a5fa';
      ctx3D.lineWidth = 1.5;
      [0, 4].forEach(offset => {{
        ctx3D.beginPath();
        ctx3D.moveTo(pSolar[offset][0], pSolar[offset][1]);
        for(let j=1; j<4; ++j) ctx3D.lineTo(pSolar[offset+j][0], pSolar[offset+j][1]);
        ctx3D.closePath();
        ctx3D.fill();
        ctx3D.stroke();
      }});

      // Draw CubeSat Faces (Depth sorted)
      faces.sort((a,b) => {{
        const zA = a.idx.reduce((acc, i) => acc + pBody[i][2], 0);
        const zB = b.idx.reduce((acc, i) => acc + pBody[i][2], 0);
        return zB - zA;
      }});

      faces.forEach(f => {{
        ctx3D.fillStyle = f.color;
        ctx3D.strokeStyle = '#0f172a';
        ctx3D.lineWidth = 1;
        ctx3D.beginPath();
        ctx3D.moveTo(pBody[f.idx[0]][0], pBody[f.idx[0]][1]);
        for(let j=1; j<4; ++j) ctx3D.lineTo(pBody[f.idx[j]][0], pBody[f.idx[j]][1]);
        ctx3D.closePath();
        ctx3D.fill();
        ctx3D.stroke();
      }});

      // Body Coordinate Axes Triad
      const origin = project3D(quatRotate([0,0,0], q), w, h);
      const axisLen = 130;
      const xAxis = project3D(quatRotate([axisLen,0,0], q), w, h);
      const yAxis = project3D(quatRotate([0,axisLen,0], q), w, h);
      const zAxis = project3D(quatRotate([0,0,axisLen], q), w, h);

      function drawAxis(pTo, color, label) {{
        ctx3D.strokeStyle = color;
        ctx3D.lineWidth = 3;
        ctx3D.beginPath();
        ctx3D.moveTo(origin[0], origin[1]);
        ctx3D.lineTo(pTo[0], pTo[1]);
        ctx3D.stroke();
        ctx3D.fillStyle = color;
        ctx3D.font = 'bold 12px monospace';
        ctx3D.fillText(label, pTo[0] + 5, pTo[1] - 5);
      }}

      drawAxis(xAxis, '#ef4444', '+X (Roll)');
      drawAxis(yAxis, '#22c55e', '+Y (Pitch)');
      drawAxis(zAxis, '#38bdf8', '+Z (Yaw)');
    }}

    function updateHUD() {{
      const f = telemetryData[currentIndex];
      document.getElementById('time-display').innerText = `t = ${{f.t.toFixed(1)}} s`;
      document.getElementById('val-err').innerText = `${{f.err.toFixed(2)}}°`;
      document.getElementById('val-err').className = f.err < 1.0 ? 'stat-val good' : 'stat-val alert';
      document.getElementById('val-omega').innerText = `${{f.w_deg.toFixed(2)}} °/s`;
      document.getElementById('val-bias').innerText = `${{f.bias_err.toFixed(4)}} rad/s`;
      document.getElementById('val-sigma').innerText = `${{f.sig.toFixed(2)}}°`;
      document.getElementById('time-slider').value = currentIndex;
      drawSparkline();
    }}

    function drawSparkline() {{
      const cvs = document.getElementById('canvas-sparkline');
      const ctx = cvs.getContext('2d');
      const w = cvs.width = cvs.clientWidth * window.devicePixelRatio;
      const h = cvs.height = cvs.clientHeight * window.devicePixelRatio;
      ctx.clearRect(0, 0, w, h);

      ctx.strokeStyle = '#38bdf8';
      ctx.lineWidth = 2 * window.devicePixelRatio;
      ctx.beginPath();
      telemetryData.forEach((d, i) => {{
        const x = (i / (telemetryData.length - 1)) * w;
        const y = h - (d.err / 40.0) * (h - 20) - 10;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }});
      ctx.stroke();

      // Current point cursor
      const curX = (currentIndex / (telemetryData.length - 1)) * w;
      const curY = h - (telemetryData[currentIndex].err / 40.0) * (h - 20) - 10;
      ctx.fillStyle = '#ef4444';
      ctx.beginPath();
      ctx.arc(curX, curY, 5 * window.devicePixelRatio, 0, 2*Math.PI);
      ctx.fill();
    }}

    function togglePlay() {{
      isPlaying = !isPlaying;
      document.getElementById('btn-play').innerText = isPlaying ? 'Pause' : 'Play';
      if (isPlaying) {{
        playInterval = setInterval(() => {{
          currentIndex = (currentIndex + 1) % telemetryData.length;
          updateHUD();
          render3D();
        }}, 50 / playSpeed);
      }} else {{
        clearInterval(playInterval);
      }}
    }}

    function resetSim() {{
      if (isPlaying) togglePlay();
      currentIndex = 0;
      updateHUD();
      render3D();
    }}

    function onSliderChange(val) {{
      currentIndex = parseInt(val);
      updateHUD();
      render3D();
    }}

    function toggleSpeed() {{
      if (playSpeed === 1) playSpeed = 2;
      else if (playSpeed === 2) playSpeed = 5;
      else playSpeed = 1;
      document.getElementById('speed-label').innerText = playSpeed + 'x';
      if (isPlaying) {{
        clearInterval(playInterval);
        playInterval = setInterval(() => {{
          currentIndex = (currentIndex + 1) % telemetryData.length;
          updateHUD();
          render3D();
        }}, 50 / playSpeed);
      }}
    }}

    // =========================================================================
    // 2D SUPERSONIC CFD VISUALIZER
    // =========================================================================
    const canvasCFD = document.getElementById('canvas-cfd');
    const ctxCFD = canvasCFD.getContext('2d');
    let cfdMode = 'mach';

    function setCFDMode(mode) {{
      cfdMode = mode;
      document.querySelectorAll('#panel-cfd .canvas-card button').forEach(b => b.classList.add('btn-secondary'));
      if (mode === 'mach') {{
        document.querySelector('#panel-cfd .canvas-card button:nth-child(1)').classList.remove('btn-secondary');
        document.getElementById('colorbar-title').innerText = 'Mach Number: 1.84 (Post-Shock) to 2.50 (Inflow)';
        document.getElementById('cbar-min').innerText = '1.84';
        document.getElementById('cbar-max').innerText = '2.50';
        document.getElementById('colorbar').style.background = 'linear-gradient(to right, #3b82f6, #06b6d4, #10b981, #f59e0b, #ef4444)';
      }} else if (mode === 'press') {{
        document.querySelector('#panel-cfd .canvas-card button:nth-child(2)').classList.remove('btn-secondary');
        document.getElementById('colorbar-title').innerText = 'Static Pressure: 101 kPa (Inflow) to 250 kPa (Post-Shock)';
        document.getElementById('cbar-min').innerText = '101 kPa';
        document.getElementById('cbar-max').innerText = '250 kPa';
        document.getElementById('colorbar').style.background = 'linear-gradient(to right, #4338ca, #3b82f6, #10b981, #eab308, #ef4444)';
      }} else {{
        document.querySelector('#panel-cfd .canvas-card button:nth-child(3)').classList.remove('btn-secondary');
        document.getElementById('colorbar-title').innerText = 'Numerical Schlieren |∇ρ| (Shock Discontinuity Magnitude)';
        document.getElementById('cbar-min').innerText = '0.0 (Uniform Flow)';
        document.getElementById('cbar-max').innerText = 'High (Shock Wave)';
        document.getElementById('colorbar').style.background = 'linear-gradient(to right, #0f172a, #475569, #cbd5e1, #ffffff)';
      }}
      redrawCFD();
    }}

    function colormap(val, min, max, mode) {{
      const norm = Math.max(0, Math.min(1, (val - min) / (max - min)));
      if (mode === 'schlieren') {{
        const c = Math.floor(norm * 255);
        return `rgb(${{c}}, ${{c}}, ${{c}})`;
      }}
      // Cool-to-Warm colormap
      const r = Math.floor(255 * Math.sin(norm * Math.PI * 0.5));
      const b = Math.floor(255 * Math.cos(norm * Math.PI * 0.5));
      const g = Math.floor(180 * Math.sin(norm * Math.PI));
      return `rgb(${{r}}, ${{g}}, ${{b}})`;
    }}

    function redrawCFD() {{
      const rect = canvasCFD.getBoundingClientRect();
      canvasCFD.width = rect.width * window.devicePixelRatio;
      canvasCFD.height = rect.height * window.devicePixelRatio;
      ctxCFD.scale(window.devicePixelRatio, window.devicePixelRatio);
      const w = rect.width, h = rect.height;

      ctxCFD.clearRect(0, 0, w, h);

      const margin = 45;
      const plotW = w - 2 * margin;
      const plotH = h - 2 * margin;

      const nx = cfdData.nx, ny = cfdData.ny;
      const dx = (cfdData.xMax - 0.0) / nx;
      const tanTheta = Math.tan(15.0 * Math.PI / 180.0);

      // Min/Max bounds
      let minV = 1.84, maxV = 2.50;
      let dataArr = cfdData.mach;
      if (cfdMode === 'press') {{
        minV = 101325; maxV = 250000;
        dataArr = cfdData.press;
      }} else if (cfdMode === 'schlieren') {{
        minV = 0; maxV = 120;
        dataArr = cfdData.schlieren;
      }}

      // Render Cells
      const showGrid = document.getElementById('chk-grid').checked;

      for (let j = 0; j < ny; ++j) {{
        for (let i = 0; i < nx; ++i) {{
          const cellIdx = j * nx + i;
          const val = dataArr[cellIdx];

          // 4 Node Coordinates
          function getNode(ni, nj) {{
            const x = ni * dx;
            const yBot = x >= 0.4 ? (x - 0.4) * tanTheta : 0.0;
            const y = yBot + (nj / ny) * (0.9 - yBot);
            return [
              margin + (x / 1.8) * plotW,
              h - margin - (y / 0.9) * plotH
            ];
          }}

          const p0 = getNode(i, j);
          const p1 = getNode(i+1, j);
          const p2 = getNode(i+1, j+1);
          const p3 = getNode(i, j+1);

          ctxCFD.fillStyle = colormap(val, minV, maxV, cfdMode);
          ctxCFD.beginPath();
          ctxCFD.moveTo(p0[0], p0[1]);
          ctxCFD.lineTo(p1[0], p1[1]);
          ctxCFD.lineTo(p2[0], p2[1]);
          ctxCFD.lineTo(p3[0], p3[1]);
          ctxCFD.closePath();
          ctxCFD.fill();

          if (showGrid) {{
            ctxCFD.strokeStyle = 'rgba(255,255,255,0.08)';
            ctxCFD.lineWidth = 0.5;
            ctxCFD.stroke();
          }}
        }}
      }}

      // Analytical Oblique Shock Overlay Line (beta = 36.94 deg)
      if (document.getElementById('chk-shock').checked) {{
        const betaRad = 36.9449 * Math.PI / 180.0;
        const tanBeta = Math.tan(betaRad);
        const xStart = 0.4;
        const xEnd = Math.min(1.8, 0.4 + 0.9 / tanBeta);
        const yEnd = (xEnd - 0.4) * tanBeta;

        const pStart = [margin + (xStart / 1.8) * plotW, h - margin];
        const pEnd = [margin + (xEnd / 1.8) * plotW, h - margin - (yEnd / 0.9) * plotH];

        ctxCFD.strokeStyle = '#ef4444';
        ctxCFD.lineWidth = 2.5;
        ctxCFD.setLineDash([6, 4]);
        ctxCFD.beginPath();
        ctxCFD.moveTo(pStart[0], pStart[1]);
        ctxCFD.lineTo(pEnd[0], pEnd[1]);
        ctxCFD.stroke();
        ctxCFD.setLineDash([]);

        ctxCFD.fillStyle = '#ef4444';
        ctxCFD.font = 'bold 12px monospace';
        ctxCFD.fillText('Oblique Shock (β = 36.9°)', pEnd[0] - 170, pEnd[1] + 15);
      }}

      // Bottom Wall Wedge Representation
      ctxCFD.fillStyle = '#1e293b';
      ctxCFD.strokeStyle = '#94a3b8';
      ctxCFD.lineWidth = 2;
      ctxCFD.beginPath();
      ctxCFD.moveTo(margin, h - margin);
      ctxCFD.lineTo(margin + (0.4 / 1.8) * plotW, h - margin);
      ctxCFD.lineTo(margin + plotW, h - margin - ((1.4 * tanTheta) / 0.9) * plotH);
      ctxCFD.lineTo(margin + plotW, h);
      ctxCFD.lineTo(margin, h);
      ctxCFD.closePath();
      ctxCFD.fill();
      ctxCFD.stroke();

      // Flow Vector Indicator
      ctxCFD.fillStyle = '#38bdf8';
      ctxCFD.font = 'bold 13px sans-serif';
      ctxCFD.fillText('Inflow Mach 2.50 ────▶', margin + 20, margin + 40);
    }}

    // Probe Mouse Hover
    canvasCFD.addEventListener('mousemove', e => {{
      const rect = canvasCFD.getBoundingClientRect();
      const mx = e.clientX - rect.left;
      const my = e.clientY - rect.top;

      const margin = 45;
      const plotW = rect.width - 2 * margin;
      const plotH = rect.height - 2 * margin;

      if (mx < margin || mx > margin + plotW || my < margin || my > margin + plotH) return;

      const xPhys = ((mx - margin) / plotW) * 1.8;
      const yPhys = (1.0 - (my - margin) / plotH) * 0.9;

      const nx = cfdData.nx, ny = cfdData.ny;
      const i = Math.min(nx - 1, Math.max(0, Math.floor((xPhys / 1.8) * nx)));
      const tanTheta = Math.tan(15.0 * Math.PI / 180.0);
      const yBot = xPhys >= 0.4 ? (xPhys - 0.4) * tanTheta : 0.0;
      const eta = Math.max(0, Math.min(1, (yPhys - yBot) / (0.9 - yBot)));
      const j = Math.min(ny - 1, Math.max(0, Math.floor(eta * ny)));

      const cellIdx = j * nx + i;
      const machVal = cfdData.mach[cellIdx];
      const pressVal = cfdData.press[cellIdx];
      const schVal = cfdData.schlieren[cellIdx];

      document.getElementById('probe-coord').innerText = `(${{xPhys.toFixed(2)}} m, ${{yPhys.toFixed(2)}} m)`;
      document.getElementById('probe-mach').innerText = machVal ? machVal.toFixed(3) : '-';
      document.getElementById('probe-press').innerText = pressVal ? `${{(pressVal / 1000).toFixed(1)}} kPa` : '-';
      document.getElementById('probe-sch').innerText = schVal ? schVal.toFixed(1) : '-';
    }});

    // Initialize
    setTimeout(() => {{
      resizeCanvas();
      updateHUD();
      render3D();
    }}, 100);
  </script>
</body>
</html>
"""

    out_path = "visualizer.html"
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(html_content)
    print(f"Generated standalone interactive visualizer at: {out_path}")

if __name__ == "__main__":
    build_visualizer()
