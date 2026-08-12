#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any


DEFAULT_ROVER_URL = os.environ.get("YAKROVER_URL", "http://192.168.4.53").rstrip("/")
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8787
COMMANDS = {"forward", "back", "left", "right", "stop"}
READS = {
    "info": "/info",
    "i2c": "/i2c/scan",
    "sensor": "/sensor/ht",
    "imu": "/imu/raw",
    "encoder": "/encoder/status",
    "encoder-reset": "/encoder/reset",
    "balance": "/balance/status",
    "config": "/balance/config",
}
POSTS = {
    "calibrate": "/balance/calibrate",
    "arm": "/balance/arm",
    "disarm": "/balance/disarm",
}


HTML = """<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Tumbller Balance Lab</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #0e1112;
      --surface: #171c1f;
      --surface-2: #20272c;
      --surface-3: #263038;
      --text: #f2f5f2;
      --muted: #9fafaa;
      --border: #344147;
      --accent: #4fd18b;
      --accent-2: #64b5f6;
      --danger: #ff6961;
      --warn: #f6c85f;
      --shadow: rgba(0, 0, 0, 0.28);
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      min-height: 100vh;
      background: var(--bg);
      color: var(--text);
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      letter-spacing: 0;
    }

    main {
      width: min(1320px, calc(100vw - 28px));
      margin: 0 auto;
      padding: 20px 0 28px;
    }

    header {
      display: grid;
      grid-template-columns: 1fr auto;
      gap: 16px;
      align-items: end;
      margin-bottom: 16px;
    }

    h1 {
      margin: 0;
      font-size: 34px;
      line-height: 1.05;
      font-weight: 760;
    }

    .subhead {
      margin-top: 6px;
      color: var(--muted);
      font-size: 14px;
    }

    .status-bar {
      min-width: 300px;
      display: grid;
      gap: 6px;
      justify-items: end;
      color: var(--muted);
      font-size: 13px;
    }

    .status-pill {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      border: 1px solid var(--border);
      border-radius: 999px;
      background: var(--surface);
      padding: 8px 12px;
      color: var(--text);
      box-shadow: 0 10px 26px var(--shadow);
    }

    .dot {
      width: 10px;
      height: 10px;
      border-radius: 999px;
      background: var(--warn);
      box-shadow: 0 0 0 4px rgba(246, 200, 95, 0.13);
      flex: 0 0 auto;
    }

    .dot.ok { background: var(--accent); box-shadow: 0 0 0 4px rgba(79, 209, 139, 0.13); }
    .dot.bad { background: var(--danger); box-shadow: 0 0 0 4px rgba(255, 105, 97, 0.13); }

    .grid {
      display: grid;
      grid-template-columns: minmax(320px, 0.95fr) minmax(420px, 1.45fr) minmax(300px, 0.9fr);
      gap: 14px;
      align-items: start;
    }

    section {
      border: 1px solid var(--border);
      border-radius: 8px;
      background: var(--surface);
      padding: 14px;
      box-shadow: 0 12px 30px var(--shadow);
    }

    .stack { display: grid; gap: 14px; }

    h2 {
      margin: 0 0 12px;
      font-size: 15px;
      line-height: 1.2;
      font-weight: 720;
      color: #dfe7e2;
    }

    .metric-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }

    .metric {
      min-height: 76px;
      border: 1px solid var(--border);
      border-radius: 8px;
      background: var(--surface-2);
      padding: 10px;
      display: grid;
      align-content: space-between;
      gap: 4px;
    }

    .metric.wide { grid-column: 1 / -1; }

    .label {
      color: var(--muted);
      font-size: 12px;
      line-height: 1.25;
    }

    .value {
      font: 700 24px/1 ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      color: var(--text);
      overflow-wrap: anywhere;
    }

    .value.small { font-size: 15px; line-height: 1.25; }
    .value.good { color: var(--accent); }
    .value.warn { color: var(--warn); }
    .value.bad { color: var(--danger); }

    .chart-wrap {
      min-height: 310px;
      border: 1px solid var(--border);
      border-radius: 8px;
      background: #0a0d0f;
      padding: 10px;
    }

    canvas {
      width: 100%;
      height: 284px;
      display: block;
    }

    .legend {
      display: flex;
      gap: 14px;
      align-items: center;
      color: var(--muted);
      font-size: 12px;
      margin-top: 8px;
    }

    .legend span { display: inline-flex; align-items: center; gap: 6px; }
    .swatch { width: 18px; height: 3px; border-radius: 999px; background: var(--accent); }
    .swatch.blue { background: var(--accent-2); }
    .swatch.yellow { background: var(--warn); }
    .swatch.red { background: var(--danger); }
    .swatch.purple { background: #b58cff; }

    .range-grid {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 8px;
      margin-top: 10px;
    }

    .range-cell {
      border: 1px solid var(--border);
      border-radius: 8px;
      background: var(--surface-2);
      padding: 8px;
      min-height: 56px;
    }

    .range-cell .value { font-size: 18px; }

    .pad {
      display: grid;
      grid-template-columns: repeat(3, minmax(70px, 1fr));
      grid-template-rows: repeat(3, minmax(70px, 1fr));
      gap: 8px;
      aspect-ratio: 1;
      max-height: 330px;
    }

    button, input, select {
      font: inherit;
      color: var(--text);
    }

    button {
      appearance: none;
      border: 1px solid var(--border);
      background: var(--surface-2);
      border-radius: 8px;
      min-height: 42px;
      padding: 0 12px;
      cursor: pointer;
      transition: transform 90ms ease, border-color 120ms ease, background 120ms ease;
      user-select: none;
      -webkit-tap-highlight-color: transparent;
    }

    button:hover { border-color: var(--accent); }
    button:active, button.is-active { transform: scale(0.98); background: var(--surface-3); }
    button:focus-visible, input:focus-visible, select:focus-visible { outline: 3px solid rgba(79, 209, 139, 0.28); outline-offset: 2px; }
    button:disabled { opacity: 0.46; cursor: not-allowed; }

    .move {
      font-size: 34px;
      line-height: 1;
      display: grid;
      place-items: center;
      font-weight: 760;
    }

    .forward { grid-column: 2; grid-row: 1; }
    .left { grid-column: 1; grid-row: 2; }
    .right { grid-column: 3; grid-row: 2; }
    .back { grid-column: 2; grid-row: 3; }

    .stop {
      grid-column: 2;
      grid-row: 2;
      background: var(--danger);
      border-color: color-mix(in srgb, var(--danger), white 18%);
      color: #250607;
      font-size: 18px;
      font-weight: 800;
    }

    .row {
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
      align-items: center;
    }

    .row > button { flex: 1 1 auto; }

    .toggle {
      border-color: color-mix(in srgb, var(--accent), var(--border) 55%);
      background: color-mix(in srgb, var(--accent), var(--surface-2) 82%);
    }

    .danger-outline {
      border-color: color-mix(in srgb, var(--danger), var(--border) 50%);
    }

    .readout {
      margin: 0;
      min-height: 210px;
      max-height: 320px;
      overflow: auto;
      border: 1px solid var(--border);
      border-radius: 8px;
      background: #090c0e;
      padding: 10px;
      color: #d0e8df;
      font: 12px/1.45 ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace;
      white-space: pre-wrap;
    }

    .raw-grid {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 8px;
    }

    .raw-cell {
      border: 1px solid var(--border);
      border-radius: 8px;
      background: var(--surface-2);
      padding: 8px;
    }

    .raw-cell .value { font-size: 16px; }

    .checklist {
      display: grid;
      gap: 8px;
      color: var(--muted);
      font-size: 13px;
    }

    .check {
      display: grid;
      grid-template-columns: 18px 1fr;
      gap: 8px;
      align-items: start;
    }

    .check input {
      margin: 2px 0 0;
      width: 16px;
      height: 16px;
      accent-color: var(--accent);
    }

    .pid-grid {
      display: grid;
      gap: 8px;
    }

    .pid-row {
      display: grid;
      grid-template-columns: 70px 1fr;
      gap: 8px;
      align-items: center;
    }

    .pid-row input,
    .pid-row select {
      width: 100%;
      min-height: 38px;
      border: 1px solid var(--border);
      border-radius: 8px;
      background: #101518;
      padding: 0 10px;
      font: 14px/1 ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    }

    .note {
      color: var(--muted);
      font-size: 12px;
      line-height: 1.45;
      margin: 10px 0 0;
    }

    @media (max-width: 1080px) {
      .grid { grid-template-columns: 1fr 1fr; }
      .chart-section { grid-column: 1 / -1; }
    }

    @media (max-width: 740px) {
      main { width: min(100vw - 18px, 520px); padding-top: 14px; }
      header { grid-template-columns: 1fr; align-items: start; }
      .status-bar { min-width: 0; width: 100%; justify-items: start; }
      .grid { grid-template-columns: 1fr; }
      section { padding: 12px; }
      h1 { font-size: 28px; }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <div>
        <h1>Tumbller Balance Lab</h1>
        <div class="subhead">Sensor validation and bring-up dashboard</div>
      </div>
      <div class="status-bar">
        <div class="status-pill"><span class="dot" id="dot" aria-hidden="true"></span><strong id="state">Connecting</strong></div>
        <div id="target"></div>
      </div>
    </header>

    <div class="grid">
      <div class="stack">
        <section>
          <h2>Live Balance</h2>
          <div class="metric-grid">
            <div class="metric">
              <div class="label">Pitch</div>
              <div class="value" id="pitch">--</div>
              <div class="label" id="pitchSource">Kalman source --</div>
            </div>
            <div class="metric">
              <div class="label">Accel Pitch</div>
              <div class="value" id="accelPitch">--</div>
              <div class="label" id="accelPitchSource">source --</div>
            </div>
            <div class="metric">
              <div class="label">Gyro Rate</div>
              <div class="value" id="gyroRate">--</div>
              <div class="label" id="gyroRateSource">source --</div>
            </div>
            <div class="metric">
              <div class="label">Loop dt</div>
              <div class="value" id="loopDt">--</div>
            </div>
            <div class="metric wide">
              <div class="label">IMU</div>
              <div class="value small" id="imuState">--</div>
            </div>
          </div>
        </section>

        <section>
          <h2>Raw IMU</h2>
          <div class="raw-grid">
            <div class="raw-cell"><div class="label">AX</div><div class="value" id="ax">--</div></div>
            <div class="raw-cell"><div class="label">AY</div><div class="value" id="ay">--</div></div>
            <div class="raw-cell"><div class="label">AZ</div><div class="value" id="az">--</div></div>
            <div class="raw-cell"><div class="label">GX</div><div class="value" id="gx">--</div></div>
            <div class="raw-cell"><div class="label">GY</div><div class="value" id="gy">--</div></div>
            <div class="raw-cell"><div class="label">GZ</div><div class="value" id="gz">--</div></div>
          </div>
        </section>

        <section>
          <h2>Encoders</h2>
          <div class="metric-grid">
            <div class="metric">
              <div class="label">Totals L / R</div>
              <div class="value small" id="encoderTotals">--</div>
            </div>
            <div class="metric">
              <div class="label">40 ms delta L / R</div>
              <div class="value small" id="encoderDeltas">--</div>
            </div>
            <div class="metric">
              <div class="label">Rate L / R</div>
              <div class="value small" id="encoderRates">--</div>
            </div>
            <div class="metric">
              <div class="label">Speed filter</div>
              <div class="value" id="encoderSpeedFilter">--</div>
            </div>
            <div class="metric">
              <div class="label">Balance speed delta</div>
              <div class="value small" id="speedLoopDeltas">--</div>
            </div>
            <div class="metric">
              <div class="label">Speed-loop output</div>
              <div class="value small" id="speedLoopOutput">--</div>
            </div>
          </div>
          <div class="row" style="margin-top: 10px;">
            <button id="resetEncoders">Reset Encoders</button>
          </div>
          <p class="note">Encoder speed uses Elegoo's 40 ms window shape. Speed-loop output affects motors only when runtime speed mix is on.</p>
        </section>

        <section>
          <h2>Axis Candidates</h2>
          <div class="metric-grid">
            <div class="metric">
              <div class="label">atan2(AY, AZ)</div>
              <div class="value" id="angleAyAz">--</div>
              <div class="label" id="angleAyAzRaw">raw --</div>
            </div>
            <div class="metric">
              <div class="label">atan2(AX, AZ)</div>
              <div class="value" id="angleAxAz">--</div>
              <div class="label" id="angleAxAzRaw">raw --</div>
            </div>
            <div class="metric">
              <div class="label">X tilt atan2(AX, √AY²+AZ²)</div>
              <div class="value" id="tiltX">--</div>
              <div class="label" id="tiltXRaw">raw --</div>
            </div>
            <div class="metric">
              <div class="label">atan2(AX, AY)</div>
              <div class="value" id="angleAxAy">--</div>
              <div class="label" id="angleAxAyRaw">raw --</div>
            </div>
            <div class="metric">
              <div class="label">Gyro X/Y/Z</div>
              <div class="value small" id="gyroAxes">--</div>
            </div>
          </div>
          <p class="note">The large numbers are smoothed. The correct balance candidate changes during forward/back tilt, not side-to-side wheel lift.</p>
        </section>
      </div>

      <section class="chart-section">
        <h2>Axis Movement Trace</h2>
        <div class="chart-wrap">
          <canvas id="chart" width="840" height="284"></canvas>
          <div class="legend">
            <span><i class="swatch"></i>atan2(AY, AZ)</span>
            <span><i class="swatch blue"></i>X tilt</span>
            <span><i class="swatch yellow"></i>atan2(AX, AY)</span>
          </div>
        </div>
        <div class="range-grid">
          <div class="range-cell">
            <div class="label">AY/AZ movement</div>
            <div class="value" id="rangeAyAz">--</div>
          </div>
          <div class="range-cell">
            <div class="label">X tilt movement</div>
            <div class="value" id="rangeTiltX">--</div>
          </div>
          <div class="range-cell">
            <div class="label">AX/AY movement</div>
            <div class="value" id="rangeAxAy">--</div>
          </div>
        </div>
        <div class="row" style="margin-top: 12px;">
          <button id="poll">Pause Polling</button>
          <button id="calibrate" class="toggle">Calibrate Gyro</button>
          <button id="clearChart">Clear Trace</button>
        </div>
        <p class="note" id="movementHint">Clear the trace, then rock only forward/back. The largest movement number is the best pitch-axis candidate for that motion.</p>

        <h2 style="margin-top: 18px;">Gyro Rate Trace</h2>
        <div class="chart-wrap">
          <canvas id="gyroChart" width="840" height="284"></canvas>
          <div class="legend">
            <span><i class="swatch blue"></i>gyro X</span>
            <span><i class="swatch yellow"></i>gyro Y</span>
            <span><i class="swatch red"></i>gyro Z</span>
          </div>
        </div>
        <div class="range-grid">
          <div class="range-cell">
            <div class="label">gyro X movement</div>
            <div class="value" id="rangeGyroX">--</div>
          </div>
          <div class="range-cell">
            <div class="label">gyro Y movement</div>
            <div class="value" id="rangeGyroY">--</div>
          </div>
          <div class="range-cell">
            <div class="label">gyro Z movement</div>
            <div class="value" id="rangeGyroZ">--</div>
          </div>
        </div>
        <p class="note" id="gyroHint">For forward/back pitch, use the gyro axis with the largest smooth bidirectional rate response while X tilt is moving.</p>
      </section>

      <div class="stack">
        <section>
          <h2>Motor Pad</h2>
          <div class="pad" aria-label="Movement controls">
            <button class="move forward" data-command="forward" title="Forward" aria-label="Forward">↑</button>
            <button class="move left" data-command="left" title="Left" aria-label="Left">←</button>
            <button class="move stop" data-command="stop" title="Stop" aria-label="Stop">STOP</button>
            <button class="move right" data-command="right" title="Right" aria-label="Right">→</button>
            <button class="move back" data-command="back" title="Back" aria-label="Back">↓</button>
          </div>
          <div class="row" style="margin-top: 10px;">
            <button id="hold" class="danger-outline">Hold Stop</button>
          </div>
        </section>

        <section>
          <h2>Orientation Test</h2>
          <div class="checklist">
            <label class="check"><input type="checkbox"><span>Wheels-down upright, held near balance</span></label>
            <label class="check"><input type="checkbox"><span>Tilt forward and note pitch direction</span></label>
            <label class="check"><input type="checkbox"><span>Tilt backward and note pitch direction</span></label>
            <label class="check"><input type="checkbox"><span>Tilt left and right; confirm pitch mostly ignores roll</span></label>
            <label class="check"><input type="checkbox"><span>Wheels-up inverted reference</span></label>
          </div>
        </section>

        <section>
          <h2>Balance Control Preview</h2>
          <div class="pid-grid">
            <div class="metric">
              <div class="label">Control gate</div>
              <div class="value small" id="controlGate">--</div>
            </div>
            <div class="metric">
              <div class="label">Arm state</div>
              <div class="value small" id="armState">--</div>
            </div>
            <div class="metric">
              <div class="label">Arm timer</div>
              <div class="value small" id="armTimer">--</div>
            </div>
            <div class="metric">
              <div class="label">Safety reason</div>
              <div class="value small" id="controlSafety">--</div>
            </div>
            <div class="metric">
              <div class="label">Kp / Ki / Kd</div>
              <div class="value small" id="pidGains">--</div>
            </div>
            <div class="metric">
              <div class="label">P / I / D terms</div>
              <div class="value small" id="pidTerms">--</div>
            </div>
            <div class="metric">
              <div class="label">Angle error</div>
              <div class="value" id="balanceError">--</div>
            </div>
            <div class="metric">
              <div class="label">Proposed output</div>
              <div class="value" id="balanceOutput">--</div>
            </div>
            <div class="metric">
              <div class="label">Mixed output</div>
              <div class="value" id="mixedOutput">--</div>
            </div>
            <div class="metric">
              <div class="label">Run saturation</div>
              <div class="value small" id="saturationStats">--</div>
            </div>
            <div class="metric">
              <div class="label">Output movement</div>
              <div class="value" id="balanceOutputRange">--</div>
            </div>
            <div class="metric">
              <div class="label">Output min / max</div>
              <div class="value small" id="balanceOutputMinMax">--</div>
            </div>
            <div class="metric">
              <div class="label">Drive command</div>
              <div class="value small" id="driveCommand">--</div>
            </div>
            <div class="metric wide">
              <div class="label">Trace safety samples</div>
              <div class="value small" id="balanceSafetyStats">--</div>
            </div>
          </div>
          <div class="pid-grid" style="margin-top: 10px;">
            <label class="pid-row"><span>Kp</span><input id="gainKp" inputmode="decimal" value="5.0"></label>
            <label class="pid-row"><span>Ki</span><input id="gainKi" inputmode="decimal" value="0.0"></label>
            <label class="pid-row"><span>Kd</span><input id="gainKd" inputmode="decimal" value="0.15"></label>
            <label class="pid-row"><span>Limit</span><input id="gainLimit" inputmode="decimal" value="120"></label>
            <label class="pid-row"><span>Setpoint</span><input id="gainSetpoint" inputmode="decimal" value="0.0"></label>
            <label class="pid-row"><span>Max angle</span><input id="gainMaxAngle" inputmode="decimal" value="22"></label>
            <label class="pid-row"><span>Motor sign</span><select id="gainMotorSign"><option value="1">+1</option><option value="-1">-1</option></select></label>
            <label class="pid-row"><span>Speed mix</span><select id="gainSpeedMix"><option value="0">off</option><option value="1">on</option></select></label>
            <label class="pid-row"><span>Mix scale</span><input id="gainSpeedScale" inputmode="decimal" value="0.05"></label>
            <label class="pid-row"><span>Arm ms</span><input id="armDuration" inputmode="numeric" value="1500"></label>
          </div>
          <div class="row" style="margin-top: 10px;">
            <button id="profileTether">Angle-only Tether</button>
            <button id="profileSpeedMix">Low Speed Mix</button>
          </div>
          <div class="row" style="margin-top: 10px;">
            <button id="applyGains" class="toggle">Apply Runtime Config</button>
            <button id="readConfig">Read Config</button>
          </div>
          <div class="row" style="margin-top: 10px;">
            <button id="armTimedBalance" class="danger-outline">Arm Timed</button>
            <button id="armBalance" class="danger-outline">Arm Untimed</button>
            <button id="disarmBalance" class="stop">DISARM / STOP</button>
          </div>
          <p class="note">Use timed arm for physical trials. Space or Escape disarms balance and sends motor stop.</p>
        </section>

        <section>
          <h2>Tether Test Process</h2>
          <div class="checklist">
            <label class="check"><input type="checkbox"><span>Tether catches the robot before the body hits the floor; wheels still touch freely.</span></label>
            <label class="check"><input type="checkbox"><span>Calibrate while stationary, upright, and slack on the tether.</span></label>
            <label class="check"><input type="checkbox"><span>Start with Angle-only Tether, speed mix off, Ki zero, 1000-1500 ms arm.</span></label>
            <label class="check"><input type="checkbox"><span>Watch physical behavior first: correct direction, underpowered, oscillating, or saturating.</span></label>
            <label class="check"><input type="checkbox"><span>Only enable Low Speed Mix after angle-only correction pushes the right way.</span></label>
          </div>
        </section>
      </div>
    </div>

    <section style="margin-top: 14px;">
      <h2>Endpoint Readout</h2>
      <div class="row">
        <button data-read="info">Info</button>
        <button data-read="i2c">I2C</button>
        <button data-read="imu">IMU Raw</button>
        <button data-read="encoder">Encoders</button>
        <button data-read="encoder-reset">Reset Encoders</button>
        <button data-read="balance">Balance</button>
        <button data-read="sensor">SHT3x</button>
      </div>
      <pre class="readout" id="readout">Ready.</pre>
    </section>
  </main>

  <script>
    const state = document.getElementById("state");
    const target = document.getElementById("target");
    const readout = document.getElementById("readout");
    const dot = document.getElementById("dot");
    const holdButton = document.getElementById("hold");
    const pollButton = document.getElementById("poll");
    const calibrateButton = document.getElementById("calibrate");
    const clearChartButton = document.getElementById("clearChart");
    const applyGainsButton = document.getElementById("applyGains");
    const readConfigButton = document.getElementById("readConfig");
    const armBalanceButton = document.getElementById("armBalance");
    const armTimedBalanceButton = document.getElementById("armTimedBalance");
    const disarmBalanceButton = document.getElementById("disarmBalance");
    const resetEncodersButton = document.getElementById("resetEncoders");
    const profileTetherButton = document.getElementById("profileTether");
    const profileSpeedMixButton = document.getElementById("profileSpeedMix");
    const chart = document.getElementById("chart");
    const ctx = chart.getContext("2d");
    const gyroChart = document.getElementById("gyroChart");
    const gyroCtx = gyroChart.getContext("2d");

    let holdTimer = null;
    let pressState = null;
    let polling = true;
    let pollTimer = null;
    let latestBalance = null;
    let latestImu = null;
    let latestEncoder = null;
    const samples = [];
    const maxSamples = 180;
    const holdDelayMs = 220;
    const holdRepeatMs = 300;

    target.textContent = window.location.origin;

    function setState(text, kind = "") {
      state.textContent = text;
      dot.className = `dot ${kind}`;
    }

    function fmt(value, digits = 2, suffix = "") {
      if (value === null || value === undefined || Number.isNaN(Number(value))) return "--";
      return `${Number(value).toFixed(digits)}${suffix}`;
    }

    function numeric(value, fallback = 0) {
      const number = Number(value);
      return Number.isFinite(number) ? number : fallback;
    }

    function candidateValue(balance, smoothedKey, rawKey) {
      return numeric(balance[smoothedKey], numeric(balance[rawKey], 0));
    }

    function setText(id, value) {
      document.getElementById(id).textContent = value;
    }

    async function api(path, options = {}) {
      const response = await fetch(path, options);
      const text = await response.text();
      if (!response.ok) throw new Error(text || response.statusText);
      return text;
    }

    async function apiJson(path, options = {}) {
      return JSON.parse(await api(path, options));
    }

    async function command(name, options = {}) {
      try {
        setState(name === "stop" ? "Stopping" : `Sending ${name}`, "");
        const suffix = options.hold ? "?hold=1" : "";
        await api(`/api/motor/${name}${suffix}`, { method: "POST" });
        setState(name === "stop" ? "Stopped" : options.hold ? `Holding ${name}` : `Sent ${name}`, "ok");
      } catch (error) {
        setState("Command failed", "bad");
        readout.textContent = String(error.message || error);
      }
    }

    async function read(name) {
      try {
        setState("Reading", "");
        const text = await api(`/api/${name}`);
        try {
          readout.textContent = JSON.stringify(JSON.parse(text), null, 2);
        } catch {
          readout.textContent = text;
        }
        setState("Online", "ok");
      } catch (error) {
        setState("Offline", "bad");
        readout.textContent = String(error.message || error);
      }
    }

    async function pollTelemetry() {
      if (!polling) return;
      const [balanceResult, encoderResult] = await Promise.allSettled([
        apiJson("/api/balance"),
        apiJson("/api/encoder"),
      ]);

      if (balanceResult.status === "fulfilled") {
        latestBalance = balanceResult.value;
        updateBalanceTelemetry(latestBalance);
        pushSample(latestBalance);
      }

      if (encoderResult.status === "fulfilled") {
        latestEncoder = encoderResult.value;
        updateEncoderTelemetry(latestEncoder);
      }

      if (balanceResult.status === "fulfilled") {
        setState(latestBalance.lastReadOk ? "Telemetry live" : "IMU read warning", latestBalance.lastReadOk ? "ok" : "bad");
      } else {
        setState("Telemetry offline", "bad");
      }
    }

    function updateBalanceTelemetry(balance) {
      setText("pitch", fmt(balance.pitchDeg, 2, "°"));
      setText("accelPitch", fmt(balance.accelPitchDeg, 2, "°"));
      setText("gyroRate", fmt(balance.gyroRateDps, 3, " dps"));
      setText("pitchSource", `Kalman ${balance.accelPitchSource || "angle"} + ${balance.gyroRateSource || "rate"}`);
      setText("accelPitchSource", `source ${balance.accelPitchSource || "unknown"}`);
      setText("gyroRateSource", `source ${balance.gyroRateSource || "unknown"}`);
      setText("loopDt", fmt(balance.loopDtMs, 3, " ms"));
      const compat = balance.whoAmICompatible ? "compatible" : "advisory";
      setText("imuState", `${balance.imuAddress} / WHO ${balance.whoAmI} / ${compat}`);
      setText("angleAyAz", fmt(balance.accelAngleAyAzSmoothedDeg ?? balance.accelAngleAyAzDeg, 2, "°"));
      setText("angleAxAz", fmt(balance.accelAngleAxAzSmoothedDeg ?? balance.accelAngleAxAzDeg, 2, "°"));
      setText("tiltX", fmt(balance.accelTiltXSmoothedDeg ?? balance.accelTiltXDeg, 2, "°"));
      setText("angleAxAy", fmt(balance.accelAngleAxAySmoothedDeg ?? balance.accelAngleAxAyDeg, 2, "°"));
      setText("angleAyAzRaw", `raw ${fmt(balance.accelAngleAyAzDeg, 2, "°")}`);
      setText("angleAxAzRaw", `raw ${fmt(balance.accelAngleAxAzDeg, 2, "°")}`);
      setText("tiltXRaw", `raw ${fmt(balance.accelTiltXDeg, 2, "°")}`);
      setText("angleAxAyRaw", `raw ${fmt(balance.accelAngleAxAyDeg, 2, "°")}`);
      setText("gyroAxes", `X ${fmt(balance.gyroXRateDps, 2)} / Y ${fmt(balance.gyroYRateDps, 2)} / Z ${fmt(balance.gyroZRateDps, 2)}`);
      setText("controlGate", `compute ${balance.balanceControllerEnabled ? "on" : "off"} / motor ${balance.balanceMotorOutputAvailable ? "available" : "unavailable"}`);
      const armRemaining = numeric(balance.balanceArmRemainingMs, 0);
      const armTimeout = numeric(balance.balanceArmTimeoutMs, 0);
      const armElapsed = numeric(balance.balanceArmElapsedMs, 0);
      const armTimer = armTimeout > 0
        ? `${Math.ceil(armRemaining / 100) / 10}s left / ${Math.ceil(armElapsed / 100) / 10}s elapsed`
        : balance.balanceMotorOutputArmed ? "untimed" : `${balance.balanceRunSampleCount ?? 0} samples last run`;
      setText("armState", `${balance.balanceMotorOutputArmed ? "armed" : "disarmed"} / output ${balance.balanceMotorOutputEnabled ? "on" : "off"}`);
      setText("armTimer", armTimer);
      setText("controlSafety", `${balance.balanceControlSafetyOk ? "ok" : "blocked"}: ${balance.balanceSafetyReason || "--"}`);
      setText("pidGains", `${fmt(balance.balanceKp, 2)} / ${fmt(balance.balanceKi, 2)} / ${fmt(balance.balanceKd, 2)}`);
      setText("pidTerms", `${fmt(balance.balancePTerm, 1)} / ${fmt(balance.balanceITerm, 1)} / ${fmt(balance.balanceDTerm, 1)}`);
      setText("balanceError", fmt(balance.balanceAngleErrorDeg, 2, "°"));
      setText("balanceOutput", `${fmt(balance.balanceOutputClamped, 1)} pwm`);
      setText("mixedOutput", `${fmt(balance.balanceMixedOutputClamped, 1)} pwm`);
      setText("saturationStats", `${fmt(balance.balanceMixedOutputSaturationRatio, 3)} / ${balance.balanceMixedOutputSaturationCount ?? 0} hits / ${balance.balanceRunSampleCount ?? 0} samples`);
      setText("driveCommand", `${balance.balanceDriveCommandSent ? "sent" : "--"} / sign ${fmt(balance.balanceMotorSign, 0)}`);
      setText("speedLoopDeltas", `${balance.speedLoopDeltaLeft ?? "--"} / ${balance.speedLoopDeltaRight ?? "--"}`);
      setText("speedLoopOutput", `${fmt(balance.speedLoopOutput, 1)} pwm / mix ${balance.speedLoopMixEnabled ? "on" : "off"} @ ${fmt(balance.speedLoopMixScale, 2)}`);
      syncGainInputs(balance);
    }

    function updateRawImuTelemetry(imu) {
      const raw = imu.raw || {};
      ["ax", "ay", "az", "gx", "gy", "gz"].forEach((key) => setText(key, raw[key] ?? "--"));
    }

    function updateEncoderTelemetry(encoder) {
      setText("encoderTotals", `${encoder.totalLeft ?? "--"} / ${encoder.totalRight ?? "--"}`);
      setText("encoderDeltas", `${encoder.deltaLeft ?? "--"} / ${encoder.deltaRight ?? "--"}`);
      setText("encoderRates", `${fmt(encoder.leftRatePps, 1)} / ${fmt(encoder.rightRatePps, 1)} pps`);
      setText("encoderSpeedFilter", fmt(encoder.speedFilter, 2));
    }

    function pushSample(balance) {
      samples.push({
        ayaz: candidateValue(balance, "accelAngleAyAzSmoothedDeg", "accelAngleAyAzDeg"),
        tiltx: candidateValue(balance, "accelTiltXSmoothedDeg", "accelTiltXDeg"),
        axay: candidateValue(balance, "accelAngleAxAySmoothedDeg", "accelAngleAxAyDeg"),
        gyrox: numeric(balance.gyroXRateDps, 0),
        gyroy: numeric(balance.gyroYRateDps, 0),
        gyroz: numeric(balance.gyroZRateDps, 0),
        output: numeric(balance.balanceOutputClamped, 0),
        safetyOk: Boolean(balance.balanceControlSafetyOk),
      });
      if (samples.length > maxSamples) samples.shift();
      drawCharts();
      updateMovementStats();
    }

    function drawCharts() {
      drawAngleChart();
      drawGyroChart();
    }

    function drawFrame(canvasElement, context, scaleLabel) {
      const w = canvasElement.width;
      const h = canvasElement.height;
      context.clearRect(0, 0, w, h);
      context.fillStyle = "#090c0e";
      context.fillRect(0, 0, w, h);

      context.strokeStyle = "#223039";
      context.lineWidth = 1;
      context.beginPath();
      for (let y = 32; y < h; y += 44) {
        context.moveTo(0, y);
        context.lineTo(w, y);
      }
      context.stroke();

      context.strokeStyle = "#526168";
      context.beginPath();
      context.moveTo(0, h / 2);
      context.lineTo(w, h / 2);
      context.stroke();

      context.fillStyle = "#9fafaa";
      context.font = "12px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace";
      context.fillText(scaleLabel, 12, 18);
    }

    function drawAngleChart() {
      const w = chart.width;
      const h = chart.height;
      const range = movementChartRange();
      drawFrame(chart, ctx, `+/-${range.toFixed(0)} deg from trace start`);

      drawDeltaSeries(chart, ctx, "ayaz", "#4fd18b", range, true);
      drawDeltaSeries(chart, ctx, "tiltx", "#64b5f6", range, true);
      drawDeltaSeries(chart, ctx, "axay", "#f6c85f", range, true);
    }

    function movementChartRange() {
      if (samples.length < 2) return 5;
      const keys = ["ayaz", "tiltx", "axay"];
      const maxDelta = keys.reduce((max, key) => {
        const baseline = samples[0][key];
        return Math.max(max, ...samples.map((sample) => Math.abs(angleDelta(sample[key], baseline))));
      }, 0);
      return Math.max(5, Math.min(180, Math.ceil(maxDelta / 5) * 5));
    }

    function angleDelta(value, baseline) {
      let delta = value - baseline;
      while (delta > 180) delta -= 360;
      while (delta < -180) delta += 360;
      return delta;
    }

    function drawGyroChart() {
      const range = gyroChartRange();
      drawFrame(gyroChart, gyroCtx, `+/-${range.toFixed(0)} dps from trace start`);

      drawDeltaSeries(gyroChart, gyroCtx, "gyrox", "#64b5f6", range, false);
      drawDeltaSeries(gyroChart, gyroCtx, "gyroy", "#f6c85f", range, false);
      drawDeltaSeries(gyroChart, gyroCtx, "gyroz", "#ff6961", range, false);
    }

    function gyroChartRange() {
      if (samples.length < 2) return 5;
      const keys = ["gyrox", "gyroy", "gyroz"];
      const maxDelta = keys.reduce((max, key) => {
        const baseline = samples[0][key];
        return Math.max(max, ...samples.map((sample) => Math.abs(sample[key] - baseline)));
      }, 0);
      return Math.max(5, Math.min(500, Math.ceil(maxDelta / 5) * 5));
    }

    function drawDeltaSeries(canvasElement, context, key, color, range, wrapAngles) {
      if (samples.length < 2) return;
      const w = canvasElement.width;
      const h = canvasElement.height;
      const mid = h / 2;
      const step = w / Math.max(1, maxSamples - 1);
      context.strokeStyle = color;
      context.lineWidth = 2;
      context.beginPath();
      const baseline = samples[0][key];
      samples.forEach((sample, index) => {
        const x = index * step;
        const delta = wrapAngles ? angleDelta(sample[key], baseline) : sample[key] - baseline;
        const y = mid - Math.max(-range, Math.min(range, delta)) / range * (h * 0.42);
        if (index === 0) context.moveTo(x, y);
        else context.lineTo(x, y);
      });
      context.stroke();
    }

    function updateMovementStats() {
      const ranges = {
        ayaz: movementRange("ayaz"),
        tiltx: movementRange("tiltx"),
        axay: movementRange("axay"),
        gyrox: linearMovementRange("gyrox"),
        gyroy: linearMovementRange("gyroy"),
        gyroz: linearMovementRange("gyroz"),
        output: linearMovementRange("output"),
      };
      setText("rangeAyAz", fmt(ranges.ayaz, 2, "°"));
      setText("rangeTiltX", fmt(ranges.tiltx, 2, "°"));
      setText("rangeAxAy", fmt(ranges.axay, 2, "°"));
      setText("rangeGyroX", fmt(ranges.gyrox, 2, " dps"));
      setText("rangeGyroY", fmt(ranges.gyroy, 2, " dps"));
      setText("rangeGyroZ", fmt(ranges.gyroz, 2, " dps"));
      setText("balanceOutputRange", fmt(ranges.output, 1, " pwm"));
      setText("balanceOutputMinMax", outputMinMaxText());
      setText("balanceSafetyStats", safetyStatsText());

      const controlCandidates = { ayaz: ranges.ayaz, tiltx: ranges.tiltx };
      const dominant = Object.entries(controlCandidates).sort((a, b) => b[1] - a[1])[0];
      const labels = { ayaz: "AY/AZ", tiltx: "X tilt" };
      if (ranges.axay > 120) {
        setText("movementHint", "AX/AY is wrapping near upright; ignore yellow for pitch selection. Compare X tilt against AY/AZ for the control candidate.");
      } else if (dominant && dominant[1] > 2) {
        setText("movementHint", `${labels[dominant[0]]} is the strongest non-wrapping candidate in this trace window. Clear the trace between forward/back and side-to-side tests.`);
      } else {
        setText("movementHint", "Clear the trace, then rock only forward/back. Prefer the non-wrapping candidate that moves smoothly for pitch.");
      }

      const gyroDominant = Object.entries({ gyrox: ranges.gyrox, gyroy: ranges.gyroy, gyroz: ranges.gyroz }).sort((a, b) => b[1] - a[1])[0];
      const gyroLabels = { gyrox: "gyro X", gyroy: "gyro Y", gyroz: "gyro Z" };
      if (gyroDominant && gyroDominant[1] > 2) {
        setText("gyroHint", `${gyroLabels[gyroDominant[0]]} has the largest rate movement in this trace. Confirm it is smooth and changes sign when you reverse forward/back tilt.`);
      } else {
        setText("gyroHint", "Clear the trace, then rock only forward/back. The matching gyro axis should move clearly while X tilt moves.");
      }
    }

    function movementRange(key) {
      if (samples.length < 2) return 0;
      const baseline = samples[0][key];
      const deltas = samples.map((sample) => angleDelta(sample[key], baseline));
      return Math.max(...deltas) - Math.min(...deltas);
    }

    function linearMovementRange(key) {
      if (samples.length < 2) return 0;
      const baseline = samples[0][key];
      const deltas = samples.map((sample) => sample[key] - baseline);
      return Math.max(...deltas) - Math.min(...deltas);
    }

    function outputMinMaxText() {
      if (samples.length < 2) return "--";
      const values = samples.map((sample) => sample.output);
      return `${fmt(Math.min(...values), 1)} / ${fmt(Math.max(...values), 1)} pwm`;
    }

    function safetyStatsText() {
      if (samples.length < 2) return "--";
      const blocked = samples.filter((sample) => !sample.safetyOk).length;
      return `${samples.length - blocked} ok / ${blocked} blocked`;
    }

    function syncGainInputs(balance) {
      const fields = [
        ["gainKp", balance.balanceKp],
        ["gainKi", balance.balanceKi],
        ["gainKd", balance.balanceKd],
        ["gainLimit", balance.balanceOutputLimit],
        ["gainSetpoint", balance.balanceSetpointDeg],
        ["gainMaxAngle", balance.balanceMaxAbsAngleDeg],
        ["gainSpeedScale", balance.speedLoopMixScale],
      ];
      fields.forEach(([id, value]) => {
        const input = document.getElementById(id);
        if (!input || document.activeElement === input) return;
        input.value = fmt(value, id === "gainLimit" ? 0 : 2);
      });
      const signInput = document.getElementById("gainMotorSign");
      if (signInput && document.activeElement !== signInput) {
        signInput.value = Number(balance.balanceMotorSign) < 0 ? "-1" : "1";
      }
      const speedMixInput = document.getElementById("gainSpeedMix");
      if (speedMixInput && document.activeElement !== speedMixInput) {
        speedMixInput.value = balance.speedLoopMixEnabled ? "1" : "0";
      }
    }

    async function applyPreviewGains() {
      const params = new URLSearchParams({
        kp: document.getElementById("gainKp").value,
        ki: document.getElementById("gainKi").value,
        kd: document.getElementById("gainKd").value,
        limit: document.getElementById("gainLimit").value,
        setpoint: document.getElementById("gainSetpoint").value,
        maxAngle: document.getElementById("gainMaxAngle").value,
        motorSign: document.getElementById("gainMotorSign").value,
        speedMix: document.getElementById("gainSpeedMix").value,
        speedScale: document.getElementById("gainSpeedScale").value,
      });
      try {
        setState("Applying config", "");
        const result = await apiJson(`/api/config?${params.toString()}`);
        readout.textContent = JSON.stringify(result, null, 2);
        samples.length = 0;
        drawCharts();
        updateMovementStats();
        setState("Config applied", "ok");
      } catch (error) {
        setState("Config failed", "bad");
        readout.textContent = String(error.message || error);
      }
    }

    async function armBalance(timed = false) {
      try {
        setState(timed ? "Arming timed" : "Arming balance", "");
        const duration = Math.max(0, Math.min(5000, Math.round(numeric(document.getElementById("armDuration").value, 1500))));
        const path = timed ? `/api/arm?ms=${duration}` : "/api/arm";
        const result = await apiJson(path, { method: "POST" });
        readout.textContent = JSON.stringify(result, null, 2);
        setState(result.armed ? (timed ? `Armed ${duration} ms` : "Balance armed") : "Arm blocked", result.armed ? "ok" : "bad");
      } catch (error) {
        setState("Arm blocked", "bad");
        readout.textContent = String(error.message || error);
      }
    }

    const runtimeProfiles = {
      tether: { kp: "5.0", ki: "0.0", kd: "0.15", limit: "45", setpoint: "0.0", maxAngle: "18", motorSign: "1", speedMix: "0", speedScale: "0.05", armMs: "1500" },
      speedMix: { kp: "5.0", ki: "0.0", kd: "0.15", limit: "45", setpoint: "0.0", maxAngle: "22", motorSign: "1", speedMix: "1", speedScale: "0.05", armMs: "1500" },
    };

    async function applyRuntimeProfile(name) {
      const profile = runtimeProfiles[name];
      if (!profile) return;
      document.getElementById("gainKp").value = profile.kp;
      document.getElementById("gainKi").value = profile.ki;
      document.getElementById("gainKd").value = profile.kd;
      document.getElementById("gainLimit").value = profile.limit;
      document.getElementById("gainSetpoint").value = profile.setpoint;
      document.getElementById("gainMaxAngle").value = profile.maxAngle;
      document.getElementById("gainMotorSign").value = profile.motorSign;
      document.getElementById("gainSpeedMix").value = profile.speedMix;
      document.getElementById("gainSpeedScale").value = profile.speedScale;
      document.getElementById("armDuration").value = profile.armMs;
      await applyPreviewGains();
    }

    async function panicStop() {
      clearPress(false);
      activeKeys.forEach((timer) => clearInterval(timer));
      activeKeys.clear();
      document.querySelectorAll("[data-command]").forEach((button) => button.classList.remove("is-active"));
      setState("Disarming", "");
      const results = await Promise.allSettled([
        apiJson("/api/disarm", { method: "POST" }),
        api("/api/motor/stop", { method: "POST" }),
      ]);
      const failed = results.find((result) => result.status === "rejected");
      if (failed) {
        setState("Stop warning", "bad");
        readout.textContent = String(failed.reason?.message || failed.reason || "stop failed");
      } else {
        setState("Disarmed / stopped", "ok");
        readout.textContent = JSON.stringify(results[0].value, null, 2);
      }
    }

    function setActiveButton(commandName, active) {
      document.querySelectorAll(`[data-command="${commandName}"]`).forEach((button) => {
        button.classList.toggle("is-active", active);
      });
    }

    function clearPress(sendStop) {
      if (!pressState) return;
      const { name, button, pointerId, startedAt, delayTimer, repeatTimer, didHold } = pressState;
      const elapsed = Date.now() - startedAt;
      clearTimeout(delayTimer);
      clearInterval(repeatTimer);
      setActiveButton(name, false);
      pressState = null;
      if (name !== "stop") {
        if (sendStop && didHold) panicStop();
        else if (!didHold && elapsed < holdDelayMs) command(name);
      }
      if (button && button.releasePointerCapture) {
        try { button.releasePointerCapture(pointerId); } catch {}
      }
    }

    function beginPress(button, pointerId) {
      const name = button.dataset.command;
      if (!name) return;
      if (pressState) clearPress(true);
      setActiveButton(name, true);

      let repeatTimer = null;
      const delayTimer = setTimeout(() => {
        if (!pressState || pressState.name !== name) return;
        pressState.didHold = true;
        command(name, { hold: true });
        repeatTimer = setInterval(() => command(name, { hold: true }), holdRepeatMs);
        if (pressState && pressState.name === name) pressState.repeatTimer = repeatTimer;
      }, holdDelayMs);

      pressState = { name, button, pointerId, startedAt: Date.now(), delayTimer, repeatTimer, didHold: false };

      if (button.setPointerCapture && pointerId !== undefined) {
        try { button.setPointerCapture(pointerId); } catch {}
      }
    }

    document.querySelectorAll("[data-command]").forEach((button) => {
      button.addEventListener("pointerdown", (event) => {
        event.preventDefault();
        beginPress(button, event.pointerId);
      });
      button.addEventListener("pointerup", (event) => {
        event.preventDefault();
        clearPress(true);
      });
      button.addEventListener("pointercancel", () => clearPress(true));
      button.addEventListener("lostpointercapture", () => clearPress(true));
      button.addEventListener("click", (event) => event.preventDefault());
    });

    document.querySelectorAll("[data-read]").forEach((button) => {
      button.addEventListener("click", () => read(button.dataset.read));
    });

    holdButton.addEventListener("click", () => {
      if (holdTimer) {
        clearInterval(holdTimer);
        holdTimer = null;
        holdButton.textContent = "Hold Stop";
        setState("Hold released", "");
        return;
      }
      panicStop();
      holdTimer = setInterval(() => panicStop(), 300);
      holdButton.textContent = "Release Hold";
      setState("Holding stop", "ok");
    });

    pollButton.addEventListener("click", () => {
      polling = !polling;
      pollButton.textContent = polling ? "Pause Polling" : "Resume Polling";
      setState(polling ? "Polling resumed" : "Polling paused", polling ? "ok" : "");
    });

    calibrateButton.addEventListener("click", async () => {
      try {
        setState("Calibrating", "");
        const result = await apiJson("/api/calibrate", { method: "POST" });
        readout.textContent = JSON.stringify(result, null, 2);
        setState("Calibration queued", "ok");
      } catch (error) {
        setState("Calibration failed", "bad");
        readout.textContent = String(error.message || error);
      }
    });

    clearChartButton.addEventListener("click", () => {
      samples.length = 0;
      drawCharts();
      updateMovementStats();
    });

    applyGainsButton.addEventListener("click", applyPreviewGains);
    readConfigButton.addEventListener("click", () => read("config"));
    profileTetherButton.addEventListener("click", () => applyRuntimeProfile("tether"));
    profileSpeedMixButton.addEventListener("click", () => applyRuntimeProfile("speedMix"));
    armTimedBalanceButton.addEventListener("click", () => armBalance(true));
    armBalanceButton.addEventListener("click", () => armBalance(false));
    disarmBalanceButton.addEventListener("click", panicStop);
    resetEncodersButton.addEventListener("click", () => read("encoder-reset"));

    const keys = new Map([
      ["w", "forward"],
      ["arrowup", "forward"],
      ["s", "back"],
      ["arrowdown", "back"],
      ["a", "left"],
      ["arrowleft", "left"],
      ["d", "right"],
      ["arrowright", "right"],
      [" ", "stop"],
      ["escape", "stop"],
    ]);

    const activeKeys = new Map();

    window.addEventListener("keydown", (event) => {
      const name = keys.get(event.key.toLowerCase());
      if (!name || event.repeat) return;
      event.preventDefault();
      if (name === "stop") {
        panicStop();
        return;
      }
      setActiveButton(name, true);
      command(name, { hold: name !== "stop" });
      if (name !== "stop") {
        activeKeys.set(name, setInterval(() => command(name, { hold: true }), holdRepeatMs));
      }
    });

    window.addEventListener("keyup", (event) => {
      const name = keys.get(event.key.toLowerCase());
      if (!name) return;
      if (name === "stop") {
        event.preventDefault();
        return;
      }
      const timer = activeKeys.get(name);
      if (timer) clearInterval(timer);
      activeKeys.delete(name);
      setActiveButton(name, false);
      if (name !== "stop") command("stop");
    });

    drawCharts();
    pollTelemetry();
    pollTimer = setInterval(pollTelemetry, 1000);
    read("info");
  </script>
</body>
</html>
"""


def rover_request(rover_url: str, path: str, timeout: float = 2.0) -> tuple[int, str, str]:
    url = f"{rover_url}{path}"
    try:
        with urllib.request.urlopen(url, timeout=timeout) as response:
            body = response.read().decode("utf-8", errors="replace")
            content_type = response.headers.get("Content-Type", "text/plain")
            return response.status, content_type, body
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        return exc.code, "text/plain", body
    except Exception as exc:
        return 502, "text/plain", str(exc)


class ControlHandler(BaseHTTPRequestHandler):
    rover_url = DEFAULT_ROVER_URL

    def log_message(self, format: str, *args: Any) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), format % args))

    def send_text(self, status: int, body: str, content_type: str = "text/plain") -> None:
        encoded = body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", f"{content_type}; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def do_GET(self) -> None:
        if self.path == "/" or self.path == "/index.html":
            self.send_text(200, HTML, "text/html")
            return
        if self.path.startswith("/api/"):
            self.handle_api()
            return
        self.send_text(404, "Not found")

    def do_POST(self) -> None:
        if self.path.startswith("/api/"):
            self.handle_api()
            return
        self.send_text(404, "Not found")

    def handle_api(self) -> None:
        if self.path.startswith("/api/motor/"):
            api_path, _, query = self.path.partition("?")
            command = api_path.rsplit("/", 1)[-1]
            if command not in COMMANDS:
                self.send_text(400, "Unknown motor command")
                return
            suffix = "?hold=1" if "hold=1" in query else ""
            status, content_type, body = rover_request(self.rover_url, f"/motor/{command}{suffix}")
            self.send_text(status, body, content_type)
            return

        action_name = self.path.removeprefix("/api/").split("?", 1)[0]
        if action_name in READS:
            _, _, query = self.path.partition("?")
            suffix = f"?{query}" if query else ""
            status, content_type, body = rover_request(self.rover_url, f"{READS[action_name]}{suffix}")
            self.send_text(status, body, content_type)
            return
        if action_name in POSTS:
            _, _, query = self.path.partition("?")
            suffix = f"?{query}" if query else ""
            status, content_type, body = rover_request(self.rover_url, f"{POSTS[action_name]}{suffix}")
            self.send_text(status, body, content_type)
            return

        self.send_text(404, "Unknown API path")


def serve(args: argparse.Namespace) -> None:
    ControlHandler.rover_url = args.rover_url.rstrip("/")
    server = ThreadingHTTPServer((args.host, args.port), ControlHandler)
    print(f"Tumbller Balance Lab: http://{args.host}:{args.port}")
    print(f"Rover target: {ControlHandler.rover_url}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping.")


def cli(args: argparse.Namespace) -> None:
    command = args.command
    rover_url = args.rover_url.rstrip("/")
    if command in COMMANDS:
        status, _content_type, body = rover_request(rover_url, f"/motor/{command}")
    elif command in READS:
        status, _content_type, body = rover_request(rover_url, READS[command])
    elif command in POSTS:
        status, _content_type, body = rover_request(rover_url, POSTS[command])
    else:
        raise SystemExit(f"Unknown command: {command}")

    if body:
        try:
            print(json.dumps(json.loads(body), indent=2))
        except json.JSONDecodeError:
            print(body)
    if status >= 400:
        raise SystemExit(status)


def main() -> None:
    parser = argparse.ArgumentParser(description="Tumbller balance validation dashboard and CLI.")
    parser.add_argument("--rover-url", default=DEFAULT_ROVER_URL)
    subparsers = parser.add_subparsers(dest="mode")

    serve_parser = subparsers.add_parser("serve", help="start the visual dashboard")
    serve_parser.add_argument("--host", default=DEFAULT_HOST)
    serve_parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    serve_parser.set_defaults(func=serve)

    cli_parser = subparsers.add_parser("send", help="send a single command")
    cli_parser.add_argument("command", choices=sorted(COMMANDS | set(READS) | set(POSTS)))
    cli_parser.set_defaults(func=cli)

    args = parser.parse_args()
    if not hasattr(args, "func"):
        args = parser.parse_args(["serve"])
    args.func(args)


if __name__ == "__main__":
    main()
