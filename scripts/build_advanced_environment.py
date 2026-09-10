#!/usr/bin/env python3
"""
Advanced Space Environment Generator
Generates visualizer.html:
- Pure black deep-space aesthetic
- Realistic 3D Earth globe with NASA atmospheric texture and diurnal rotation
- Highly detailed satellite model (gold MLI, solar wings, high-gain dish, optical telescope)
- Interactive 2D World Map + 3D Earth ground targeting with red tracking beacon
- Real-time closed-loop attitude slew & target tracking
- Multi-camera director (Free Orbit, Chase Cam, Telescope Lookdown, Global)
- Strict ECI (Inertial) vs ECEF (Earth-Fixed) reference frame transformation
- Ground Station Elevation Angle & Line-of-Sight (LOS) horizon occultation
- Orbit Umbra / Eclipse solar illumination detection
- Westward-drifting orbital ground track trail
"""

import os
import json

def build_advanced_visualizer():
    print("Reading Three.js and textures...")
    with open('three.min.js', 'r', encoding='utf-8') as f:
        three_js = f.read()

    with open('OrbitControls.js', 'r', encoding='utf-8') as f:
        orbit_controls_js = f.read()

    with open('earth_b64.txt', 'r', encoding='utf-8') as f:
        earth_b64 = f.read().strip()

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Dynamics & Ground Tracking Environment</title>
  <style>
    :root {{
      --bg: #000000;
      --panel-bg: rgba(6, 9, 15, 0.85);
      --border: rgba(255, 255, 255, 0.12);
      --border-focus: rgba(56, 189, 248, 0.6);
      --text: #f1f5f9;
      --muted: #64748b;
      --cyan: #38bdf8;
      --red: #ef4444;
      --green: #10b981;
      --amber: #f59e0b;
    }}
    * {{ box-sizing: border-box; margin: 0; padding: 0; user-select: none; }}
    body {{
      background: var(--bg);
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", monospace;
      overflow: hidden;
      width: 100vw;
      height: 100vh;
    }}
    #webgl-canvas {{
      position: absolute;
      top: 0; left: 0;
      width: 100vw; height: 100vh;
      z-index: 1;
    }}

    /* UI Panels (Stealth Aerospace Style) */
    .hud-panel {{
      position: absolute;
      background: var(--panel-bg);
      border: 1px solid var(--border);
      backdrop-filter: blur(10px);
      border-radius: 6px;
      padding: 14px 16px;
      font-size: 12px;
      z-index: 10;
      box-shadow: 0 8px 32px rgba(0, 0, 0, 0.8);
      pointer-events: auto;
    }}

    /* Header Bar */
    .header-bar {{
      position: absolute;
      top: 16px; left: 20px; right: 20px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      z-index: 10;
      pointer-events: none;
    }}
    .brand-title {{
      font-size: 13px;
      font-weight: 700;
      letter-spacing: 1.5px;
      text-transform: uppercase;
      color: #fff;
      display: flex;
      align-items: center;
      gap: 10px;
      background: var(--panel-bg);
      border: 1px solid var(--border);
      padding: 8px 14px;
      border-radius: 4px;
      backdrop-filter: blur(8px);
      pointer-events: auto;
    }}
    .status-dot {{
      width: 8px; height: 8px;
      border-radius: 50%;
      background: var(--green);
      box-shadow: 0 0 10px var(--green);
    }}
    .status-dot.tracking {{
      background: var(--cyan);
      box-shadow: 0 0 10px var(--cyan);
      animation: pulse 1.2s infinite ease-in-out;
    }}
    .status-dot.occulted {{
      background: var(--red);
      box-shadow: 0 0 10px var(--red);
    }}
    @keyframes pulse {{
      0%, 100% {{ opacity: 0.4; transform: scale(0.9); }}
      50% {{ opacity: 1; transform: scale(1.15); }}
    }}

    /* Top Right: Telemetry HUD */
    #telemetry-panel {{
      top: 70px; right: 20px;
      width: 330px;
    }}
    .panel-header {{
      font-size: 11px;
      font-weight: 700;
      letter-spacing: 1px;
      text-transform: uppercase;
      color: var(--muted);
      margin-bottom: 12px;
      padding-bottom: 6px;
      border-bottom: 1px solid var(--border);
      display: flex;
      justify-content: space-between;
    }}
    .telem-row {{
      display: flex;
      justify-content: space-between;
      margin-bottom: 7px;
      font-family: "SFMono-Regular", Menlo, Monaco, Consolas, monospace;
    }}
    .telem-label {{ color: var(--muted); }}
    .telem-val {{ font-weight: 600; color: var(--cyan); }}
    .telem-val.red {{ color: var(--red); }}
    .telem-val.green {{ color: var(--green); }}
    .telem-val.amber {{ color: var(--amber); }}

    /* Bottom Left: Interactive Map */
    #map-panel {{
      bottom: 20px; left: 20px;
      width: 380px;
    }}
    #map-canvas {{
      width: 100%;
      height: 180px;
      background: #040810;
      border: 1px solid var(--border);
      border-radius: 4px;
      cursor: crosshair;
      display: block;
    }}
    .preset-btn-group {{
      display: flex;
      flex-wrap: wrap;
      gap: 5px;
      margin-top: 10px;
    }}
    .tiny-btn {{
      background: rgba(255,255,255,0.06);
      border: 1px solid var(--border);
      color: var(--text);
      padding: 4px 8px;
      border-radius: 3px;
      font-size: 10px;
      font-family: monospace;
      cursor: pointer;
      transition: all 0.15s;
    }}
    .tiny-btn:hover {{
      background: rgba(56, 189, 248, 0.2);
      border-color: var(--cyan);
    }}
    .tiny-btn.active {{
      background: var(--cyan);
      color: #000;
      font-weight: bold;
    }}

    /* Bottom Right: Mission Control Bar */
    #control-panel {{
      bottom: 20px; right: 20px;
      width: 330px;
    }}
    .btn-row {{
      display: flex;
      gap: 6px;
      margin-bottom: 10px;
    }}
    .action-btn {{
      flex: 1;
      background: rgba(255, 255, 255, 0.08);
      border: 1px solid var(--border);
      color: var(--text);
      padding: 7px 10px;
      border-radius: 4px;
      font-size: 11px;
      font-weight: 600;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 6px;
      transition: all 0.15s;
    }}
    .action-btn:hover {{
      background: rgba(56, 189, 248, 0.2);
      border-color: var(--cyan);
    }}
    .action-btn.active {{
      background: var(--cyan);
      color: #000;
      border-color: var(--cyan);
    }}
    .slider-row {{
      display: flex;
      align-items: center;
      gap: 8px;
      margin-bottom: 8px;
      font-size: 11px;
    }}
    .slider-row label {{ width: 95px; color: var(--muted); }}
    .slider-row input[type=range] {{
      flex: 1;
      accent-color: var(--cyan);
      cursor: pointer;
    }}
    .slider-val {{ width: 50px; text-align: right; font-family: monospace; color: var(--cyan); }}

    .action-dropdown {{
      background: rgba(8, 14, 26, 0.95);
      border: 1px solid var(--border-focus);
      color: var(--cyan);
      padding: 6px 10px;
      border-radius: 4px;
      font-size: 11px;
      font-family: monospace;
      outline: none;
      cursor: pointer;
    }}
    .action-dropdown option {{
      background: #06090f;
      color: #f1f5f9;
    }}
    .flycam-hint {{
      position: absolute;
      top: 70px;
      left: 20px;
      background: var(--panel-bg);
      border: 1px solid var(--border);
      border-radius: 6px;
      padding: 10px 14px;
      font-size: 11px;
      color: var(--muted);
      z-index: 10;
      pointer-events: none;
      backdrop-filter: blur(8px);
      line-height: 1.5;
    }}
    .flycam-hint b {{ color: var(--cyan); }}

    /* Target Reticle in 3D */
    .crosshair-overlay {{
      position: absolute;
      top: 50%; left: 50%;
      transform: translate(-50%, -50%);
      pointer-events: none;
      opacity: 0;
      transition: opacity 0.3s;
      z-index: 5;
      text-align: center;
    }}
    .crosshair-overlay.visible {{ opacity: 0.9; }}
  </style>
</head>
<body>

  <!-- WebGL Render Canvas -->
  <canvas id="webgl-canvas"></canvas>

  <!-- Mission Header -->
  <div class="header-bar">
    <div class="brand-title">
      <div id="status-light" class="status-dot tracking"></div>
      <span>GNC Tracking</span>
      <span style="color: var(--muted); font-size: 11px; font-weight: normal;">| DYNAMIC ECI / ECEF TRACKING</span>
    </div>
    <div style="display: flex; gap: 8px; align-items: center; pointer-events: auto;">
      <span style="font-size: 11px; color: var(--muted); font-weight: 600;">PERSPECTIVE [1-9]:</span>
      <select id="camera-select" class="action-dropdown" onchange="setCameraMode(this.value)">
        <option value="flycam">📷 [1] Free Flycam (WASD + Right-Click)</option>
        <option value="orbit" selected>📷 [2] Free Orbit Arcball (Left-Click)</option>
        <option value="chase">📷 [3] Orbit Chase Cam (Over Shoulder)</option>
        <option value="lookdown">📷 [4] Telescope Sensor Lookdown (Reticle)</option>
        <option value="nadir">📷 [5] Nadir Earth View</option>
        <option value="wing">📷 [6] Solar Wingtip View</option>
        <option value="ground">📷 [7] Ground Station POV (Skyward Look)</option>
        <option value="plane">📷 [8] Orbital Plane Normal</option>
        <option value="sun">📷 [9] Sun-Earth Ecliptic Line</option>
      </select>
    </div>
  </div>

  <!-- Flycam Controls Overlay Hint -->
  <div id="flycam-hint" class="flycam-hint">
    <div><b>CAMERA &amp; FLIGHT CONTROLS:</b></div>
    <div>• <b>Right-Click + Drag</b>: Look Around (Yaw &amp; Pitch)</div>
    <div>• <b>W / A / S / D</b>: Flycam Move (Forward/Left/Back/Right)</div>
    <div>• <b>Space / E</b>: Up | <b>C / Q</b>: Down | <b>Shift</b>: Boost</div>
    <div>• <b>Ctrl + Left Drag</b>: Spin Earth directly</div>
    <div>• <b>Keys 1-9</b>: Switch Camera Perspectives</div>
  </div>

  <!-- Telemetry HUD Panel -->
  <div id="telemetry-panel" class="hud-panel">
    <div class="panel-header">
      <span>Orbital & GNC Telemetry</span>
      <span id="lock-badge" style="color: var(--cyan); font-weight: bold;">● TRACKING</span>
    </div>
    <div class="telem-row">
      <span class="telem-label">Target Name:</span>
      <span id="hud-target-name" class="telem-val">Cape Canaveral</span>
    </div>
    <div class="telem-row">
      <span class="telem-label">Target (ECEF):</span>
      <span id="hud-target-coords" class="telem-val">28.39° N, 80.60° W</span>
    </div>
    <div class="telem-row">
      <span class="telem-label">Target Solar Regime:</span>
      <span id="hud-target-sun" class="telem-val">LOCAL DAY</span>
    </div>
    <div class="telem-row">
      <span class="telem-label">Sub-Sat (ECEF):</span>
      <span id="hud-sat-coords" class="telem-val">28.39° N, 80.60° W</span>
    </div>
    <div class="telem-row">
      <span class="telem-label">Line-of-Sight (LOS):</span>
      <span id="hud-los-status" class="telem-val green">ACQUIRED (+45.2°)</span>
    </div>
    <div class="telem-row">
      <span class="telem-label">Slant Range:</span>
      <span id="hud-distance" class="telem-val">682.4 km</span>
    </div>
    <div class="telem-row">
      <span class="telem-label">Spacecraft Solar:</span>
      <span id="hud-solar-regime" class="telem-val">100% [SUNLIT]</span>
    </div>
    <div class="telem-row">
      <span class="telem-label">Payload Slew Error:</span>
      <span id="hud-slew-err" class="telem-val green">0.08°</span>
    </div>
    <div class="telem-row">
      <span class="telem-label">Angular Rate |ω|:</span>
      <span id="hud-omega" class="telem-val">0.051 °/s</span>
    </div>
    <div class="telem-row">
      <span class="telem-label">Reaction Wheel:</span>
      <span id="hud-rw-speed" class="telem-val">1420 RPM</span>
    </div>
    <div class="telem-row">
      <span class="telem-label">MEKF Covariance:</span>
      <span id="hud-mekf" class="telem-val green">CONVERGED (3σ: 0.04°)</span>
    </div>
  </div>

  <!-- Interactive 2D World Map -->
  <div id="map-panel" class="hud-panel">
    <div class="panel-header">
      <span>Ground Target & Orbit Track</span>
      <span style="color: var(--muted); font-size: 10px;">CLICK MAP OR GLOBE</span>
    </div>
    <canvas id="map-canvas" width="380" height="180"></canvas>
    <div class="preset-btn-group">
      <button class="tiny-btn active" onclick="selectPreset('canaveral')">Canaveral</button>
      <button class="tiny-btn" onclick="selectPreset('houston')">Houston</button>
      <button class="tiny-btn" onclick="selectPreset('london')">London</button>
      <button class="tiny-btn" onclick="selectPreset('tokyo')">Tokyo</button>
      <button class="tiny-btn" onclick="selectPreset('sydney')">Sydney</button>
      <button class="tiny-btn" onclick="selectPreset('svalbard')">Svalbard</button>
      <button class="tiny-btn" onclick="selectPreset('kourou')">Kourou</button>
    </div>
  </div>

  <!-- Mission Controls -->
  <div id="control-panel" class="hud-panel">
    <div class="panel-header">
      <span>Flight Simulation Parameters</span>
    </div>
    <div class="btn-row">
      <button id="btn-toggle-sim" class="action-btn" onclick="toggleSim()">Pause Sim</button>
      <button class="action-btn" onclick="injectDisturbance()">Inject Tumble</button>
    </div>

    <!-- Earth Rotation Controls -->
    <div style="margin-top: 10px; margin-bottom: 6px; font-size: 11px; font-weight: 700; color: var(--cyan); letter-spacing: 0.5px;">
      EARTH ROTATION &amp; SPIN
    </div>
    <div class="slider-row">
      <label>Auto Diurnal Spin:</label>
      <input type="checkbox" id="check-auto-spin" checked onchange="toggleAutoSpin(this.checked)" style="accent-color: var(--cyan); cursor: pointer; width: 16px; height: 16px;">
      <span id="label-auto-spin" style="color: var(--cyan); font-weight: 600; font-size: 11px;">15°/h Diurnal</span>
    </div>
    <div class="slider-row">
      <label>Manual Angle:</label>
      <input type="range" id="slider-earth-rot" min="0" max="360" value="0" step="0.5" oninput="onEarthRotChange(this.value)">
      <span id="val-earth-rot" class="slider-val">0.0°</span>
    </div>
    <div class="btn-row">
      <button class="action-btn" onclick="rotateEarthDeg(15)">Turn East (+15°)</button>
      <button class="action-btn" onclick="rotateEarthDeg(-15)">Turn West (-15°)</button>
      <button class="action-btn" onclick="centerCity()">Center Target</button>
    </div>

    <!-- Simulation Parameters -->
    <div style="margin-top: 10px; margin-bottom: 6px; font-size: 11px; font-weight: 700; color: var(--cyan); letter-spacing: 0.5px;">
      ORBIT &amp; TIME DYNAMICS
    </div>
    <div class="slider-row">
      <label>Time Warp:</label>
      <input type="range" id="slider-warp" min="1" max="50" value="5" oninput="onWarpChange(this.value)">
      <span id="val-warp" class="slider-val">5x</span>
    </div>
    <div class="slider-row">
      <label>Spin Warp:</label>
      <input type="range" id="slider-spin" min="0" max="30" value="5" oninput="onSpinChange(this.value)">
      <span id="val-spin" class="slider-val">5x</span>
    </div>
    <div class="slider-row">
      <label>Orbit Altitude:</label>
      <input type="range" id="slider-alt" min="400" max="1500" value="650" oninput="onAltChange(this.value)">
      <span id="val-alt" class="slider-val">650 km</span>
    </div>
    <div class="slider-row">
      <label>Orbit Inclination:</label>
      <input type="range" id="slider-inc" min="0" max="98" value="51" oninput="onIncChange(this.value)">
      <span id="val-inc" class="slider-val">51.6°</span>
    </div>
    <div style="font-size: 10px; color: var(--muted); margin-top: 8px; line-height: 1.4;">
      <b style="color: var(--cyan)">PHYSICS NOTE:</b> Target is fixed to Earth crust (ECEF). Holding Ctrl + Left-Drag spins Earth manually. Right-Click + Drag looks around.
    </div>
  </div>

  <!-- Telescope Reticle Overlay -->
  <div id="reticle" class="crosshair-overlay">
    <svg width="220" height="220" viewBox="0 0 220 220">
      <circle cx="110" cy="110" r="80" stroke="#38bdf8" stroke-width="1.5" fill="none" stroke-dasharray="8 4"/>
      <circle cx="110" cy="110" r="35" stroke="#38bdf8" stroke-width="1" fill="none"/>
      <line x1="110" y1="15" x2="110" y2="55" stroke="#38bdf8" stroke-width="1.5"/>
      <line x1="110" y1="165" x2="110" y2="205" stroke="#38bdf8" stroke-width="1.5"/>
      <line x1="15" y1="110" x2="55" y2="110" stroke="#38bdf8" stroke-width="1.5"/>
      <line x1="165" y1="110" x2="205" y2="110" stroke="#38bdf8" stroke-width="1.5"/>
      <circle cx="110" cy="110" r="3" fill="#ef4444"/>
    </svg>
    <div id="reticle-text" style="color: var(--cyan); font-family: monospace; font-size: 11px; text-align: center; margin-top: 4px; text-shadow: 0 0 8px rgba(0,0,0,0.8);">
      RANGE: --- KM | SLEW ERR: ---°
    </div>
  </div>

  <!-- Embedded Three.js & OrbitControls -->
  <script>
{three_js}
  </script>
  <script>
{orbit_controls_js}
  </script>

  <!-- Application Engine -->
  <script>
    const earthB64 = "{earth_b64}";

    // Target Presets
    const presets = {{
      canaveral: {{ name: "Cape Canaveral", lat: 28.39, lon: -80.60 }},
      houston:   {{ name: "Houston Control", lat: 29.56, lon: -95.09 }},
      london:    {{ name: "London", lat: 51.50, lon: -0.12 }},
      tokyo:     {{ name: "Tokyo", lat: 35.68, lon: 139.76 }},
      sydney:    {{ name: "Sydney", lat: -33.86, lon: 151.20 }},
      svalbard:  {{ name: "Svalbard Station", lat: 78.22, lon: 15.65 }},
      kourou:    {{ name: "Kourou (ESA)", lat: 5.16, lon: -52.65 }}
    }};

    let targetLat = presets.canaveral.lat;
    let targetLon = presets.canaveral.lon;
    let targetName = presets.canaveral.name;

    // Simulation State
    let isRunning = true;
    let timeWarp = 5;
    let earthSpinWarp = 5;
    let orbitAltitudeKm = 650;
    let orbitIncDeg = 51.6;
    let orbitTrueAnomaly = 0.0;

    // Camera Perspectives & Flycam State
    let cameraMode = 'orbit'; // 'flycam', 'orbit', 'chase', 'lookdown', 'nadir', 'wing', 'ground', 'plane', 'sun'
    let flycamYaw = 0.0;
    let flycamPitch = -0.25;
    let isRightClicking = false;
    let lastRightMouseX = 0, lastRightMouseY = 0;
    const keysDown = {{}};

    // Earth Rotation State
    let earthSpinAngle = 0.0;
    let autoEarthSpin = true;
    let isCtrlDragging = false;
    let lastLeftMouseX = 0;

    const EARTH_RADIUS_UNITS = 30.0; // 3D units scale (1 unit ~ 212 km)
    const KM_TO_UNITS = EARTH_RADIUS_UNITS / 6378.137;

    // Set up WebGL Scene
    const canvas = document.getElementById('webgl-canvas');
    const renderer = new THREE.WebGLRenderer({{ canvas, antialias: true, alpha: false }});
    renderer.setPixelRatio(window.devicePixelRatio);
    renderer.setSize(window.innerWidth, window.innerHeight);
    renderer.shadowMap.enabled = true;

    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x000000);

    const camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 5000);
    camera.position.set(0, 8, 25);

    const controls = new THREE.OrbitControls(camera, canvas);
    controls.enableDamping = true;
    controls.dampingFactor = 0.08;
    controls.minDistance = 3;
    controls.maxDistance = 600;

    // Keyboard controls for WASD Flycam & Shortcuts 1-9
    window.addEventListener('keydown', (e) => {{
      keysDown[e.code] = true;
      const keyMap = {{
        'Digit1': 'flycam',
        'Digit2': 'orbit',
        'Digit3': 'chase',
        'Digit4': 'lookdown',
        'Digit5': 'nadir',
        'Digit6': 'wing',
        'Digit7': 'ground',
        'Digit8': 'plane',
        'Digit9': 'sun'
      }};
      if (keyMap[e.code]) {{
        setCameraMode(keyMap[e.code]);
      }}
      if (['KeyW', 'KeyA', 'KeyS', 'KeyD'].includes(e.code) && cameraMode === 'orbit') {{
        const dir = new THREE.Vector3();
        camera.getWorldDirection(dir);
        flycamPitch = Math.asin(Math.max(-1, Math.min(1, dir.y)));
        flycamYaw = Math.atan2(dir.x, dir.z);
        setCameraMode('flycam');
      }}
    }});

    window.addEventListener('keyup', (e) => {{
      keysDown[e.code] = false;
    }});

    // Right-Click Mouselook & Ctrl+Left-Click Earth Rotation
    canvas.addEventListener('mousedown', (e) => {{
      if (e.button === 2) {{
        isRightClicking = true;
        lastRightMouseX = e.clientX;
        lastRightMouseY = e.clientY;
        if (cameraMode !== 'flycam') {{
          const dir = new THREE.Vector3();
          camera.getWorldDirection(dir);
          flycamPitch = Math.asin(Math.max(-1, Math.min(1, dir.y)));
          flycamYaw = Math.atan2(dir.x, dir.z);
          setCameraMode('flycam');
        }}
        e.preventDefault();
      }} else if (e.button === 0 && e.ctrlKey) {{
        isCtrlDragging = true;
        lastLeftMouseX = e.clientX;
        controls.enabled = false;
        e.preventDefault();
      }}
    }});

    window.addEventListener('mouseup', (e) => {{
      if (e.button === 2) {{
        isRightClicking = false;
      }}
      if (e.button === 0) {{
        isCtrlDragging = false;
        if (cameraMode === 'orbit') controls.enabled = true;
      }}
    }});

    window.addEventListener('contextmenu', (e) => {{
      e.preventDefault();
    }});

    window.addEventListener('mousemove', (e) => {{
      if (isRightClicking) {{
        const dx = e.clientX - lastRightMouseX;
        const dy = e.clientY - lastRightMouseY;
        lastRightMouseX = e.clientX;
        lastRightMouseY = e.clientY;

        const sens = 0.0035;
        flycamYaw -= dx * sens;
        flycamPitch -= dy * sens;
        flycamPitch = Math.max(-1.55, Math.min(1.55, flycamPitch));
      }} else if (isCtrlDragging) {{
        const dx = e.clientX - lastLeftMouseX;
        lastLeftMouseX = e.clientX;
        earthSpinAngle += dx * 0.008;
        earthMesh.rotation.y = earthSpinAngle;
        updateEarthSlider();
      }}
    }});

    // Lighting (Sun is fixed in Earth-Centered Inertial space)
    const sunLight = new THREE.DirectionalLight(0xffffff, 2.3);
    sunLight.position.set(150, 40, 80);
    scene.add(sunLight);

    const ambientLight = new THREE.AmbientLight(0x111c2e, 0.35);
    scene.add(ambientLight);

    // Starfield Background
    const starCount = 4500;
    const starGeo = new THREE.BufferGeometry();
    const starPos = new Float32Array(starCount * 3);
    for (let i = 0; i < starCount * 3; i += 3) {{
      const r = 900 + Math.random() * 400;
      const theta = Math.random() * Math.PI * 2;
      const phi = Math.acos((Math.random() * 2) - 1);
      starPos[i] = r * Math.sin(phi) * Math.cos(theta);
      starPos[i+1] = r * Math.sin(phi) * Math.sin(theta);
      starPos[i+2] = r * Math.cos(phi);
    }}
    starGeo.setAttribute('position', new THREE.BufferAttribute(starPos, 3));
    const starMat = new THREE.PointsMaterial({{ color: 0xffffff, size: 1.2, transparent: true, opacity: 0.85 }});
    scene.add(new THREE.Points(starGeo, starMat));

    // Earth Sphere with NASA Texture
    const earthGeo = new THREE.SphereGeometry(EARTH_RADIUS_UNITS, 64, 64);
    const earthTexture = new THREE.TextureLoader().load(earthB64);
    earthTexture.anisotropy = renderer.capabilities.getMaxAnisotropy();

    const earthMat = new THREE.MeshStandardMaterial({{
      map: earthTexture,
      roughness: 0.65,
      metalness: 0.15
    }});
    const earthMesh = new THREE.Mesh(earthGeo, earthMat);
    scene.add(earthMesh);

    // Atmospheric Rim Glow (Fresnel outer shell)
    const atmosGeo = new THREE.SphereGeometry(EARTH_RADIUS_UNITS * 1.018, 64, 64);
    const atmosMat = new THREE.MeshBasicMaterial({{
      color: 0x38bdf8,
      transparent: true,
      opacity: 0.12,
      blending: THREE.AdditiveBlending,
      side: THREE.BackSide
    }});
    const atmosMesh = new THREE.Mesh(atmosGeo, atmosMat);
    scene.add(atmosMesh);

    // Target Ground Pin: Attached directly to earthMesh (ECEF frame)
    // This guarantees it is permanently locked to London / Houston on the textured surface!
    const targetGroup = new THREE.Group();
    earthMesh.add(targetGroup);

    // Red glowing cylinder beacon
    const beaconGeo = new THREE.CylinderGeometry(0.08, 0.08, 0.8, 16);
    const beaconMat = new THREE.MeshBasicMaterial({{ color: 0xef4444 }});
    const beaconMesh = new THREE.Mesh(beaconGeo, beaconMat);
    beaconMesh.position.y = 0.4;
    targetGroup.add(beaconMesh);

    // Pulsing Rings
    const ringGeo = new THREE.RingGeometry(0.2, 0.35, 32);
    const ringMat = new THREE.MeshBasicMaterial({{ color: 0xef4444, side: THREE.DoubleSide, transparent: true, opacity: 0.8 }});
    const ringMesh = new THREE.Mesh(ringGeo, ringMat);
    ringMesh.rotation.x = Math.PI * 0.5;
    targetGroup.add(ringMesh);

    // Ground Target Position Helper (ECEF surface coordinates)
    function latLonToVector3(lat, lon, radius) {{
      const phi = (90 - lat) * (Math.PI / 180);
      const theta = (lon + 180) * (Math.PI / 180);
      return new THREE.Vector3(
        -radius * Math.sin(phi) * Math.cos(theta),
        radius * Math.cos(phi),
        radius * Math.sin(phi) * Math.sin(theta)
      );
    }}

    function updateTargetPosition() {{
      const pos = latLonToVector3(targetLat, targetLon, EARTH_RADIUS_UNITS);
      targetGroup.position.copy(pos);
      targetGroup.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), pos.clone().normalize());
    }}
    updateTargetPosition();

    // =========================================================================
    // SOPHISTICATED SPACECRAFT 3D MODEL
    // =========================================================================
    const satelliteGroup = new THREE.Group();
    scene.add(satelliteGroup);

    // 1. Main Spacecraft Bus (Gold Multi-Layer Insulation Foil)
    const busGeo = new THREE.BoxGeometry(0.7, 0.7, 1.2);
    const goldMat = new THREE.MeshStandardMaterial({{
      color: 0xd4af37,
      metalness: 0.88,
      roughness: 0.28
    }});
    const busMesh = new THREE.Mesh(busGeo, goldMat);
    satelliteGroup.add(busMesh);

    // Avionics Trim Plates
    const plateGeo = new THREE.BoxGeometry(0.72, 0.72, 0.3);
    const darkMetalMat = new THREE.MeshStandardMaterial({{ color: 0x1e293b, metalness: 0.8, roughness: 0.3 }});
    const plateMesh = new THREE.Mesh(plateGeo, darkMetalMat);
    plateMesh.position.z = -0.35;
    satelliteGroup.add(plateMesh);

    // 2. Optical Telescope Payload (Boresight along +Z)
    const lensBarrelGeo = new THREE.CylinderGeometry(0.24, 0.28, 0.5, 32);
    const lensBarrelMat = new THREE.MeshStandardMaterial({{ color: 0x0f172a, metalness: 0.9, roughness: 0.2 }});
    const lensBarrel = new THREE.Mesh(lensBarrelGeo, lensBarrelMat);
    lensBarrel.rotation.x = Math.PI * 0.5;
    lensBarrel.position.z = 0.85;
    satelliteGroup.add(lensBarrel);

    const lensGlassGeo = new THREE.SphereGeometry(0.22, 32, 16, 0, Math.PI * 2, 0, Math.PI * 0.5);
    const lensGlassMat = new THREE.MeshStandardMaterial({{
      color: 0x38bdf8,
      metalness: 0.95,
      roughness: 0.05,
      transparent: true,
      opacity: 0.7
    }});
    const lensGlass = new THREE.Mesh(lensGlassGeo, lensGlassMat);
    lensGlass.rotation.x = -Math.PI * 0.5;
    lensGlass.position.z = 1.08;
    satelliteGroup.add(lensGlass);

    // 3. Articulated Dual Solar Array Wings
    function createSolarWing(dir) {{
      const wingGroup = new THREE.Group();
      // Yoke truss
      const yokeGeo = new THREE.CylinderGeometry(0.04, 0.04, 0.5, 8);
      const yokeMat = new THREE.MeshStandardMaterial({{ color: 0x475569, metalness: 0.9, roughness: 0.3 }});
      const yoke = new THREE.Mesh(yokeGeo, yokeMat);
      yoke.rotation.z = Math.PI * 0.5;
      yoke.position.x = dir * 0.55;
      wingGroup.add(yoke);

      // 2 solar panel segments
      const panelGeo = new THREE.BoxGeometry(1.4, 0.04, 0.8);
      const cellMat = new THREE.MeshStandardMaterial({{
        color: 0x1d4ed8,
        metalness: 0.8,
        roughness: 0.25
      }});
      const panel = new THREE.Mesh(panelGeo, cellMat);
      panel.position.x = dir * 1.5;
      wingGroup.add(panel);

      return wingGroup;
    }}
    satelliteGroup.add(createSolarWing(1));
    satelliteGroup.add(createSolarWing(-1));

    // 4. High-Gain Communications Dish
    const dishGroup = new THREE.Group();
    const dishGeo = new THREE.SphereGeometry(0.42, 32, 16, 0, Math.PI * 2, 0, Math.PI * 0.5);
    const dishMat = new THREE.MeshStandardMaterial({{ color: 0xe2e8f0, metalness: 0.92, roughness: 0.2, side: THREE.DoubleSide }});
    const dish = new THREE.Mesh(dishGeo, dishMat);
    dish.rotation.x = Math.PI;
    dishGroup.add(dish);

    // Feed horn
    const feedGeo = new THREE.CylinderGeometry(0.02, 0.04, 0.28, 8);
    const feed = new THREE.Mesh(feedGeo, darkMetalMat);
    feed.position.y = -0.18;
    dishGroup.add(feed);

    dishGroup.position.set(0, 0.5, -0.4);
    dishGroup.rotation.x = -0.4;
    satelliteGroup.add(dishGroup);

    // 5. Star Trackers (2 Angled Optical Baffle Cones)
    function createStarTracker(angle) {{
      const stGeo = new THREE.ConeGeometry(0.07, 0.18, 16);
      const st = new THREE.Mesh(stGeo, darkMetalMat);
      st.position.set(angle * 0.28, -0.4, 0.2);
      st.rotation.z = angle * 0.7;
      st.rotation.x = -0.4;
      return st;
    }}
    satelliteGroup.add(createStarTracker(1));
    satelliteGroup.add(createStarTracker(-1));

    // 6. Dynamic Laser Tracking Beam (ECI Spacecraft to Ground Station)
    const lineGeo = new THREE.BufferGeometry().setFromPoints([
      new THREE.Vector3(0, 0, 0),
      new THREE.Vector3(0, 0, 10)
    ]);
    const lineMat = new THREE.LineBasicMaterial({{ color: 0xef4444, transparent: true, opacity: 0.7 }});
    const trackingLaser = new THREE.Line(lineGeo, lineMat);
    scene.add(trackingLaser);

    // Satellite Physics & Dynamic Attitude Controller State
    const satPhys = {{
      q: new THREE.Quaternion(),
      omega: new THREE.Vector3(0.002, 0.001, -0.001),
      bias: new THREE.Vector3(0.012, -0.015, 0.010),
      rwRpm: 1400,
      inertia: new THREE.Vector3(0.05, 0.04, 0.03)
    }};

    // =========================================================================
    // 2D WORLD MAP RENDERER (ECEF Ground Track)
    // =========================================================================
    const mapCanvas = document.getElementById('map-canvas');
    const mapCtx = mapCanvas.getContext('2d');
    const mapImg = new Image();
    mapImg.src = earthB64;

    const groundTrackHistory = [];

    mapImg.onload = () => draw2DMap();

    // Compute Sub-Satellite Latitude & Longitude in ECEF (Accounting for Earth's rotation)
    function getSubSatelliteCoords() {{
      const localPos = earthMesh.worldToLocal(satelliteGroup.position.clone()).normalize();
      const lat = 90 - Math.acos(Math.max(-1, Math.min(1, localPos.y))) * (180 / Math.PI);
      let lon = (Math.atan2(localPos.z, -localPos.x) * (180 / Math.PI)) - 180;
      if (lon < -180) lon += 360;
      if (lon > 180) lon -= 360;
      return {{ lat, lon }};
    }}

    function draw2DMap() {{
      const w = mapCanvas.width, h = mapCanvas.height;
      mapCtx.drawImage(mapImg, 0, 0, w, h);

      // Dark tint overlay
      mapCtx.fillStyle = 'rgba(4, 8, 16, 0.45)';
      mapCtx.fillRect(0, 0, w, h);

      // Grid lines (30 deg lat/lon)
      mapCtx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
      mapCtx.lineWidth = 0.5;
      for (let lon = -180; lon <= 180; lon += 60) {{
        const x = ((lon + 180) / 360) * w;
        mapCtx.beginPath(); mapCtx.moveTo(x, 0); mapCtx.lineTo(x, h); mapCtx.stroke();
      }}
      for (let lat = -60; lat <= 60; lat += 30) {{
        const y = ((90 - lat) / 180) * h;
        mapCtx.beginPath(); mapCtx.moveTo(0, y); mapCtx.lineTo(w, y); mapCtx.stroke();
      }}

      // Sub-satellite position
      const satCoords = getSubSatelliteCoords();

      // Record ground track history
      if (isRunning) {{
        if (groundTrackHistory.length === 0 ||
            Math.hypot(satCoords.lon - groundTrackHistory[groundTrackHistory.length-1].lon,
                       satCoords.lat - groundTrackHistory[groundTrackHistory.length-1].lat) > 1.2) {{
          groundTrackHistory.push({{ lat: satCoords.lat, lon: satCoords.lon }});
          if (groundTrackHistory.length > 200) groundTrackHistory.shift();
        }}
      }}

      // Draw Orbit Ground Track Ribbon (Westward drift visualized)
      if (groundTrackHistory.length > 1) {{
        mapCtx.strokeStyle = 'rgba(56, 189, 248, 0.45)';
        mapCtx.lineWidth = 1.4;
        mapCtx.beginPath();
        for (let i = 0; i < groundTrackHistory.length - 1; i++) {{
          const p1 = groundTrackHistory[i];
          const p2 = groundTrackHistory[i+1];
          if (Math.abs(p1.lon - p2.lon) < 160) {{
            const x1 = ((p1.lon + 180) / 360) * w;
            const y1 = ((90 - p1.lat) / 180) * h;
            const x2 = ((p2.lon + 180) / 360) * w;
            const y2 = ((90 - p2.lat) / 180) * h;
            mapCtx.moveTo(x1, y1);
            mapCtx.lineTo(x2, y2);
          }}
        }}
        mapCtx.stroke();
      }}

      // Satellite Sub-Point Position
      const satX = ((satCoords.lon + 180) / 360) * w;
      const satY = ((90 - satCoords.lat) / 180) * h;

      // Draw Satellite Icon
      mapCtx.fillStyle = '#38bdf8';
      mapCtx.beginPath();
      mapCtx.arc(satX, satY, 4.5, 0, 2 * Math.PI);
      mapCtx.fill();
      mapCtx.strokeStyle = '#38bdf8';
      mapCtx.strokeRect(satX - 8, satY - 2, 16, 4);

      // Target Ground Point Marker (Red Pulsing Dot in ECEF)
      const tgtX = ((targetLon + 180) / 360) * w;
      const tgtY = ((90 - targetLat) / 180) * h;

      mapCtx.fillStyle = '#ef4444';
      mapCtx.beginPath();
      mapCtx.arc(tgtX, tgtY, 4, 0, 2 * Math.PI);
      mapCtx.fill();

      // Pulsing radar ring on map
      const ringR = 6 + (Date.now() % 1500) / 150;
      mapCtx.strokeStyle = `rgba(239, 68, 68, ${{1.0 - ringR / 16}})`;
      mapCtx.lineWidth = 1.5;
      mapCtx.beginPath();
      mapCtx.arc(tgtX, tgtY, ringR, 0, 2 * Math.PI);
      mapCtx.stroke();

      // Line between satellite and target
      mapCtx.strokeStyle = 'rgba(239, 68, 68, 0.4)';
      mapCtx.setLineDash([4, 4]);
      mapCtx.beginPath();
      mapCtx.moveTo(satX, satY);
      mapCtx.lineTo(tgtX, tgtY);
      mapCtx.stroke();
      mapCtx.setLineDash([]);
    }}

    // Map Click Handler (Select any target coordinate on flat map)
    mapCanvas.addEventListener('click', e => {{
      const rect = mapCanvas.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;

      targetLon = (x / rect.width) * 360 - 180;
      targetLat = 90 - (y / rect.height) * 180;
      targetName = `Target ($${{targetLat.toFixed(1)}}°, $${{targetLon.toFixed(1)}}°)`;

      document.querySelectorAll('.preset-btn-group button').forEach(b => b.classList.remove('active'));
      updateTargetPosition();
      draw2DMap();
    }});

    // Raycast click directly on 3D Earth Globe
    const raycaster = new THREE.Raycaster();
    const mouse = new THREE.Vector2();

    window.addEventListener('dblclick', e => {{
      mouse.x = (e.clientX / window.innerWidth) * 2 - 1;
      mouse.y = -(e.clientY / window.innerHeight) * 2 + 1;
      raycaster.setFromCamera(mouse, camera);

      const intersects = raycaster.intersectObject(earthMesh);
      if (intersects.length > 0) {{
        // Convert world intersection point into earthMesh local space
        const localPt = earthMesh.worldToLocal(intersects[0].point.clone());
        const r = localPt.length();
        targetLat = 90 - Math.acos(Math.max(-1, Math.min(1, localPt.y / r))) * (180 / Math.PI);
        targetLon = (Math.atan2(localPt.z, -localPt.x) * (180 / Math.PI)) - 180;
        if (targetLon < -180) targetLon += 360;
        if (targetLon > 180) targetLon -= 360;
        targetName = `Target ($${{targetLat.toFixed(1)}}°, $${{targetLon.toFixed(1)}}°)`;

        document.querySelectorAll('.preset-btn-group button').forEach(b => b.classList.remove('active'));
        updateTargetPosition();
        draw2DMap();
      }}
    }});

    function selectPreset(key) {{
      const p = presets[key];
      targetLat = p.lat;
      targetLon = p.lon;
      targetName = p.name;
      document.querySelectorAll('.preset-btn-group button').forEach(b => b.classList.remove('active'));
      if (window.event && window.event.target) window.event.target.classList.add('active');
      updateTargetPosition();
      draw2DMap();
    }}

    // =========================================================================
    // REAL-TIME GNC ATTITUDE CONTROL & ORBIT PROPAGATION
    // =========================================================================
    const clock = new THREE.Clock();

    function updateSimulation(dt) {{
      if (!isRunning) return;

      // 1. Orbital Kinematics (Keplerian circular orbit in ECI)
      const orbitRadius = EARTH_RADIUS_UNITS + orbitAltitudeKm * KM_TO_UNITS;
      const orbitalSpeed = Math.sqrt(398600.44 / (6378 + orbitAltitudeKm)) / (6378 + orbitAltitudeKm); // rad/s
      orbitTrueAnomaly += orbitalSpeed * dt * timeWarp * 12.0;

      const incRad = orbitIncDeg * (Math.PI / 180);
      const satX = orbitRadius * Math.cos(orbitTrueAnomaly);
      const satY = orbitRadius * Math.sin(orbitTrueAnomaly) * Math.sin(incRad);
      const satZ = orbitRadius * Math.sin(orbitTrueAnomaly) * Math.cos(incRad);
      satelliteGroup.position.set(satX, satY, satZ);

      // Earth diurnal spin around polar axis
      if (autoEarthSpin) {{
        const baseEarthSpin = 0.00008; // rad per sim-second
        earthSpinAngle += baseEarthSpin * timeWarp * earthSpinWarp;
        if (earthSpinAngle > Math.PI * 2) earthSpinAngle -= Math.PI * 2;
        earthMesh.rotation.y = earthSpinAngle;
        updateEarthSlider();
      }} else {{
        earthMesh.rotation.y = earthSpinAngle;
      }}

      // 2. Closed-Loop GNC Pointing Solver:
      // Target world position (query from earthMesh child hierarchy in ECI space)
      const targetWorldPos = new THREE.Vector3();
      targetGroup.getWorldPosition(targetWorldPos);

      const satPos = satelliteGroup.position.clone();
      const losVec = targetWorldPos.clone().sub(satPos).normalize(); // Line of Sight in ECI

      // Ground Station Elevation Angle & Horizon Visibility Check
      const upGround = targetWorldPos.clone().normalize();
      const rhoVec = satPos.clone().sub(targetWorldPos); // Vector ground -> sat
      const slantRangeKm = rhoVec.length() / KM_TO_UNITS;
      const rhoUnit = rhoVec.clone().normalize();
      const sinElev = rhoUnit.dot(upGround);
      const elevDeg = Math.asin(Math.max(-1.0, Math.min(1.0, sinElev))) * (180 / Math.PI);
      const isTargetVisible = elevDeg > 0; // Above local horizon

      // Spacecraft Solar Regime (Sunlit vs Earth Umbra/Eclipse)
      const sunDir = sunLight.position.clone().normalize();
      const satProjSun = satPos.dot(sunDir);
      let isSunlit = true;
      if (satProjSun < 0) {{
        const perpDist = satPos.clone().sub(sunDir.clone().multiplyScalar(satProjSun)).length();
        if (perpDist < EARTH_RADIUS_UNITS) {{
          isSunlit = false;
        }}
      }}

      // Target Day/Night State
      const targetProjSun = targetWorldPos.dot(sunDir);
      const isTargetDay = targetProjSun > 0;

      // Desired attitude: Satellite +Z axis (telescope lens) aligns with losVec
      // Satellite +Y axis aligns perpendicular to orbit plane
      const desiredZ = losVec.clone();
      const upOrbit = new THREE.Vector3(0, Math.cos(incRad), -Math.sin(incRad));
      const desiredX = new THREE.Vector3().crossVectors(upOrbit, desiredZ).normalize();
      const desiredY = new THREE.Vector3().crossVectors(desiredZ, desiredX).normalize();

      const targetDCM = new THREE.Matrix4().makeBasis(desiredX, desiredY, desiredZ);
      const desiredQuat = new THREE.Quaternion().setFromRotationMatrix(targetDCM);

      // Attitude Error Quaternion: q_err = q_target^-1 * q_current
      const currentQuat = satelliteGroup.quaternion;
      const qErr = desiredQuat.clone().invert().multiply(currentQuat);

      if (qErr.w < 0) {{
        qErr.set(-qErr.x, -qErr.y, -qErr.z, -qErr.w);
      }}

      // Proportional-Derivative (PD) Attitude Slew Controller
      const Kp = 3.5;
      const Kd = 2.8;
      const errVec = new THREE.Vector3(qErr.x, qErr.y, qErr.z);

      // Commanded reaction wheel torque: tau = -2*Kp*errVec - Kd*omega
      const tauCmd = errVec.clone().multiplyScalar(-2.0 * Kp).sub(satPhys.omega.clone().multiplyScalar(Kd));

      // Angular acceleration: dw/dt = J^-1 * tau
      const dw = new THREE.Vector3(
        tauCmd.x / satPhys.inertia.x,
        tauCmd.y / satPhys.inertia.y,
        tauCmd.z / satPhys.inertia.z
      );

      satPhys.omega.addScaledVector(dw, dt * timeWarp);

      // Apply quaternion integration
      const omegaNorm = satPhys.omega.length();
      if (omegaNorm > 1e-6) {{
        const deltaQ = new THREE.Quaternion().setFromAxisAngle(satPhys.omega.clone().normalize(), omegaNorm * dt * timeWarp);
        currentQuat.multiply(deltaQ).normalize();
      }}

      // Reaction wheel momentum accumulation
      satPhys.rwRpm = Math.min(6000, 1200 + satPhys.omega.length() * 15000);

      // Optical payload muzzle point (telescope lens aperture at +Z = 1.15 in body frame)
      const muzzleWorld = new THREE.Vector3(0, 0, 1.15).applyQuaternion(satelliteGroup.quaternion).add(satPos);

      // Update Tracking Laser Beam Line (Muzzle to Ground Target)
      const points = [muzzleWorld, targetWorldPos];
      trackingLaser.geometry.setFromPoints(points);

      const slewErrorDeg = 2.0 * Math.acos(Math.min(1.0, Math.abs(qErr.w))) * (180 / Math.PI);

      if (isTargetVisible) {{
        // Visible line-of-sight: GREEN
        trackingLaser.material.color.setHex(0x10b981);
        trackingLaser.material.opacity = 0.85;
      }} else {{
        // Occulted (behind Earth): RED
        trackingLaser.material.color.setHex(0xef4444);
        trackingLaser.material.opacity = 0.75;
      }}

      // Update Pulsing Radar Target Ring
      const scale = 1.0 + 0.3 * Math.sin(Date.now() * 0.006);
      ringMesh.scale.set(scale, scale, scale);

      // 3. Update Telemetry HUD
      document.getElementById('hud-target-name').innerText = targetName;
      document.getElementById('hud-target-coords').innerText = `${{targetLat.toFixed(2)}}° N, ${{targetLon.toFixed(2)}}° E`;

      const targetSunElem = document.getElementById('hud-target-sun');
      targetSunElem.innerText = isTargetDay ? 'LOCAL DAY' : 'LOCAL NIGHT';
      targetSunElem.className = isTargetDay ? 'telem-val cyan' : 'telem-val muted';

      const subSat = getSubSatelliteCoords();
      document.getElementById('hud-sat-coords').innerText = `${{subSat.lat.toFixed(2)}}° N, ${{subSat.lon.toFixed(2)}}° E`;
      document.getElementById('hud-distance').innerText = `${{slantRangeKm.toFixed(1)}} km`;

      const losElem = document.getElementById('hud-los-status');
      if (isTargetVisible) {{
        losElem.innerText = `VISIBLE (El: +${{elevDeg.toFixed(1)}}°)`;
        losElem.className = 'telem-val green';
      }} else {{
        losElem.innerText = `OCCULTED (El: ${{elevDeg.toFixed(1)}}°)`;
        losElem.className = 'telem-val red';
      }}

      const solarElem = document.getElementById('hud-solar-regime');
      if (isSunlit) {{
        solarElem.innerText = '100% [SUNLIT]';
        solarElem.className = 'telem-val cyan';
      }} else {{
        solarElem.innerText = '0% [UMBRA / ECLIPSE]';
        solarElem.className = 'telem-val amber';
      }}

      const slewElem = document.getElementById('hud-slew-err');
      slewElem.innerText = `${{slewErrorDeg.toFixed(2)}}°`;
      slewElem.className = slewErrorDeg < 0.5 ? 'telem-val green' : 'telem-val red';

      const lockBadge = document.getElementById('lock-badge');
      const statusLight = document.getElementById('status-light');
      if (!isTargetVisible) {{
        lockBadge.innerText = '● OCCULTED';
        lockBadge.style.color = 'var(--red)';
        statusLight.className = 'status-dot occulted';
      }} else if (slewErrorDeg < 0.5) {{
        lockBadge.innerText = '● LOCKED ON';
        lockBadge.style.color = 'var(--green)';
        statusLight.className = 'status-dot';
      }} else {{
        lockBadge.innerText = '● SLEWING';
        lockBadge.style.color = 'var(--amber)';
        statusLight.className = 'status-dot tracking';
      }}

      document.getElementById('hud-omega').innerText = `${{(omegaNorm * (180 / Math.PI)).toFixed(3)}} °/s`;
      document.getElementById('hud-rw-speed').innerText = `${{Math.round(satPhys.rwRpm)}} RPM`;

      if (cameraMode === 'lookdown') {{
        const reticleTxt = document.getElementById('reticle-text');
        if (reticleTxt) {{
          reticleTxt.innerText = `RANGE: ${{slantRangeKm.toFixed(1)}} KM | SLEW ERR: ${{slewErrorDeg.toFixed(2)}}°`;
        }}
      }}

      draw2DMap();
    }}

    // Camera Director Modes (9 Presets)
    function setCameraMode(mode) {{
      cameraMode = mode;
      const sel = document.getElementById('camera-select');
      if (sel && sel.value !== mode) sel.value = mode;

      const reticle = document.getElementById('reticle');
      if (mode === 'lookdown') {{
        reticle.classList.add('visible');
      }} else {{
        reticle.classList.remove('visible');
      }}

      if (mode === 'orbit') {{
        controls.enabled = true;
        controls.target.set(0, 0, 0);
      }} else {{
        controls.enabled = false;
      }}
    }}

    function updateCamera(dt) {{
      const targetWorldPos = new THREE.Vector3();
      targetGroup.getWorldPosition(targetWorldPos);

      if (cameraMode === 'flycam') {{
        controls.enabled = false;
        let speed = 25.0; // units/sec
        if (keysDown['ShiftLeft'] || keysDown['ShiftRight']) speed *= 3.0;

        const forward = new THREE.Vector3(
          Math.sin(flycamYaw) * Math.cos(flycamPitch),
          Math.sin(flycamPitch),
          Math.cos(flycamYaw) * Math.cos(flycamPitch)
        ).normalize();

        const worldUp = new THREE.Vector3(0, 1, 0);
        const right = new THREE.Vector3().crossVectors(forward, worldUp).normalize();

        if (keysDown['KeyW']) camera.position.addScaledVector(forward, speed * dt);
        if (keysDown['KeyS']) camera.position.addScaledVector(forward, -speed * dt);
        if (keysDown['KeyA']) camera.position.addScaledVector(right, -speed * dt);
        if (keysDown['KeyD']) camera.position.addScaledVector(right, speed * dt);
        if (keysDown['Space'] || keysDown['KeyE']) camera.position.addScaledVector(worldUp, speed * dt);
        if (keysDown['KeyC'] || keysDown['KeyQ']) camera.position.addScaledVector(worldUp, -speed * dt);

        const lookAtTarget = camera.position.clone().add(forward);
        camera.lookAt(lookAtTarget);
      }} else if (cameraMode === 'orbit') {{
        controls.enabled = true;
        controls.update();
      }} else if (cameraMode === 'chase') {{
        controls.enabled = false;
        const offset = new THREE.Vector3(0, 2.2, -5.5).applyQuaternion(satelliteGroup.quaternion);
        camera.position.copy(satelliteGroup.position).add(offset);
        const lookAhead = satelliteGroup.position.clone().add(new THREE.Vector3(0, 0, 15).applyQuaternion(satelliteGroup.quaternion));
        camera.lookAt(lookAhead);
      }} else if (cameraMode === 'lookdown') {{
        controls.enabled = false;
        const muzzlePos = new THREE.Vector3(0, 0, 1.15).applyQuaternion(satelliteGroup.quaternion).add(satelliteGroup.position);
        camera.position.copy(muzzlePos);
        camera.lookAt(targetWorldPos);
      }} else if (cameraMode === 'nadir') {{
        controls.enabled = false;
        const nadirOffset = new THREE.Vector3(0, 1.2, 0).applyQuaternion(satelliteGroup.quaternion);
        camera.position.copy(satelliteGroup.position).add(nadirOffset);
        camera.lookAt(0, 0, 0);
      }} else if (cameraMode === 'wing') {{
        controls.enabled = false;
        const wingOffset = new THREE.Vector3(2.4, 0.4, -0.6).applyQuaternion(satelliteGroup.quaternion);
        camera.position.copy(satelliteGroup.position).add(wingOffset);
        camera.lookAt(satelliteGroup.position);
      }} else if (cameraMode === 'ground') {{
        controls.enabled = false;
        const zenith = targetWorldPos.clone().normalize();
        camera.position.copy(targetWorldPos).add(zenith.clone().multiplyScalar(0.2));
        camera.lookAt(satelliteGroup.position);
      }} else if (cameraMode === 'plane') {{
        controls.enabled = false;
        const incRad = orbitIncDeg * (Math.PI / 180);
        camera.position.set(0, Math.cos(incRad) * 65, -Math.sin(incRad) * 65);
        camera.lookAt(0, 0, 0);
      }} else if (cameraMode === 'sun') {{
        controls.enabled = false;
        camera.position.copy(sunLight.position.clone().normalize().multiplyScalar(75));
        camera.lookAt(0, 0, 0);
      }}
    }}

    // Earth Rotation Handlers
    function toggleAutoSpin(val) {{
      autoEarthSpin = val;
      const lbl = document.getElementById('label-auto-spin');
      if (lbl) {{
        lbl.innerText = val ? '15°/h Diurnal' : 'Paused';
        lbl.style.color = val ? 'var(--cyan)' : 'var(--muted)';
      }}
    }}

    function onEarthRotChange(val) {{
      earthSpinAngle = (parseFloat(val) * Math.PI) / 180;
      earthMesh.rotation.y = earthSpinAngle;
      const elem = document.getElementById('val-earth-rot');
      if (elem) elem.innerText = parseFloat(val).toFixed(1) + '°';
    }}

    function rotateEarthDeg(deg) {{
      earthSpinAngle += (deg * Math.PI) / 180;
      earthMesh.rotation.y = earthSpinAngle;
      updateEarthSlider();
    }}

    function centerCity() {{
      earthSpinAngle = -(targetLon * Math.PI) / 180;
      earthMesh.rotation.y = earthSpinAngle;
      updateEarthSlider();
    }}

    function updateEarthSlider() {{
      let deg = (earthSpinAngle * 180) / Math.PI;
      while (deg < 0) deg += 360;
      while (deg >= 360) deg -= 360;
      const slider = document.getElementById('slider-earth-rot');
      const val = document.getElementById('val-earth-rot');
      if (slider) slider.value = deg.toFixed(1);
      if (val) val.innerText = deg.toFixed(1) + '°';
    }}

    // Controls
    function toggleSim() {{
      isRunning = !isRunning;
      document.getElementById('btn-toggle-sim').innerText = isRunning ? 'Pause Sim' : 'Resume Sim';
    }}

    function injectDisturbance() {{
      satPhys.omega.set(
        (Math.random() - 0.5) * 0.15,
        (Math.random() - 0.5) * 0.15,
        (Math.random() - 0.5) * 0.15
      );
    }}

    function onWarpChange(val) {{
      timeWarp = parseFloat(val);
      document.getElementById('val-warp').innerText = val + 'x';
    }}

    function onSpinChange(val) {{
      earthSpinWarp = parseFloat(val);
      document.getElementById('val-spin').innerText = val + 'x';
    }}

    function onAltChange(val) {{
      orbitAltitudeKm = parseFloat(val);
      document.getElementById('val-alt').innerText = val + ' km';
    }}

    function onIncChange(val) {{
      orbitIncDeg = parseFloat(val);
      document.getElementById('val-inc').innerText = val + '°';
    }}

    // Window Resize
    window.addEventListener('resize', () => {{
      camera.aspect = window.innerWidth / window.innerHeight;
      camera.updateProjectionMatrix();
      renderer.setSize(window.innerWidth, window.innerHeight);
    }});

    // Animation Loop
    function animate() {{
      requestAnimationFrame(animate);
      const dt = Math.min(0.05, clock.getDelta());
      updateSimulation(dt);
      updateCamera(dt);
      renderer.render(scene, camera);
    }}
    animate();
  </script>
</body>
</html>
"""

    out_file = "visualizer.html"
    with open(out_file, "w", encoding="utf-8") as f:
        f.write(html)
    print(f"Generated advanced visualizer at: {out_file} ({len(html)} bytes)")

if __name__ == "__main__":
    build_advanced_visualizer()
