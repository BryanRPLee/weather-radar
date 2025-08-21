const char html_page[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>WxRadar</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: #0a0a0f;
    color: #e0ffe0;
    font-family: 'Courier New', monospace;
    display: flex;
    flex-direction: column;
    align-items: center;
    min-height: 100vh;
    padding: 8px;
  }
  h1 {
    font-size: 1.1rem;
    letter-spacing: 0.2em;
    color: #62f51f;
    margin-bottom: 6px;
  }
  #canvas-wrap { position: relative; }
  canvas { display: block; border: 1px solid #1a3a1a; border-radius: 4px; }
  #hud {
    display: flex;
    gap: 18px;
    margin-top: 8px;
    font-size: 0.78rem;
    color: #88ee88;
  }
  .hud-val { color: #ffffff; font-weight: bold; }
  #legend {
    display: flex;
    gap: 4px;
    margin-top: 6px;
    align-items: center;
    font-size: 0.7rem;
    color: #88ee88;
  }
  .leg-swatch {
    width: 18px; height: 12px;
    display: inline-block;
    border-radius: 2px;
  }
  #status { margin-top: 4px; font-size: 0.65rem; color: #446644; }
</style>
</head>
<body>
<h1>&#9992; WX RADAR</h1>

<div id="canvas-wrap">
  <canvas id="radar" width="640" height="360"></canvas>
</div>

<div id="hud">
  <span>HDG <span class="hud-val" id="hHdg">---</span>&deg;</span>
  <span>ALT <span class="hud-val" id="hAlt">---</span> ft</span>
  <span>SPD <span class="hud-val" id="hSpd">---</span> kt</span>
  <span>PITCH <span class="hud-val" id="hPitch">---</span>&deg;</span>
  <span>ROLL <span class="hud-val" id="hRoll">---</span>&deg;</span>
  <span>LAT <span class="hud-val" id="hLat">---</span></span>
  <span>LON <span class="hud-val" id="hLon">---</span></span>
</div>

<div id="legend">
  <span>dBZ:</span>
  <span class="leg-swatch" style="background:#00ecec"></span><span>&lt;20</span>
  <span class="leg-swatch" style="background:#00a000"></span><span>20</span>
  <span class="leg-swatch" style="background:#00d000"></span><span>25</span>
  <span class="leg-swatch" style="background:#ffff00"></span><span>30</span>
  <span class="leg-swatch" style="background:#e7c000"></span><span>35</span>
  <span class="leg-swatch" style="background:#ff9000"></span><span>40</span>
  <span class="leg-swatch" style="background:#ff0000"></span><span>45</span>
  <span class="leg-swatch" style="background:#d40000"></span><span>50</span>
  <span class="leg-swatch" style="background:#ff00ff"></span><span>55</span>
  <span class="leg-swatch" style="background:#9955ff"></span><span>&ge;60</span>
</div>

<div id="status">Connecting…</div>

<script>

const DBZ_LEVELS = [
  {min:  0, max: 20, color: '#00ecec'},
  {min: 20, max: 25, color: '#00a000'},
  {min: 25, max: 30, color: '#00d000'},
  {min: 30, max: 35, color: '#ffff00'},
  {min: 35, max: 40, color: '#e7c000'},
  {min: 40, max: 45, color: '#ff9000'},
  {min: 45, max: 50, color: '#ff0000'},
  {min: 50, max: 55, color: '#d40000'},
  {min: 55, max: 60, color: '#ff00ff'},
  {min: 60, max: 999,color: '#9955ff'},
];

function dbzToColor(dbz) {
  if (dbz <= 0) return null;
  for (const lvl of DBZ_LEVELS) {
    if (dbz >= lvl.min && dbz < lvl.max) return lvl.color;
  }
  return '#9955ff';
}

const canvas  = document.getElementById('radar');
const ctx     = canvas.getContext('2d');
const W       = canvas.width;
const H       = canvas.height;
const CX      = W / 2;
const CY      = H - 20;
const R_MAX   = H - 30;

let sweep     = new Array(181).fill(0);
let scanAngle = 0;

function drawBackground() {
  ctx.clearRect(0, 0, W, H);
  ctx.fillStyle = '#000a00';
  ctx.fillRect(0, 0, W, H);
}

function drawGrid() {
  ctx.save();
  ctx.strokeStyle  = '#1a4a1a';
  ctx.lineWidth    = 1;
  ctx.setLineDash([4, 4]);

  const rings = [0.25, 0.50, 0.75, 1.0];
  rings.forEach(r => {
    const pr = r * R_MAX;
    ctx.beginPath();
    ctx.arc(CX, CY, pr, Math.PI, 2 * Math.PI);
    ctx.stroke();

    ctx.fillStyle = '#2a7a2a';
    ctx.font = '10px Courier New';
    ctx.fillText((r * 100).toFixed(0) + ' NM', CX + pr + 3, CY - 4);
  });

  ctx.setLineDash([3, 6]);
  for (let deg = 0; deg <= 180; deg += 30) {
    const rad = deg * Math.PI / 180;
    ctx.beginPath();
    ctx.moveTo(CX, CY);
    ctx.lineTo(CX + R_MAX * Math.cos(Math.PI - rad),
               CY - R_MAX * Math.sin(Math.PI - rad));
    ctx.stroke();

    if (deg > 0 && deg < 180) {
      const lx = CX + (R_MAX + 12) * Math.cos(Math.PI - rad);
      const ly = CY - (R_MAX + 12) * Math.sin(Math.PI - rad);
      ctx.fillStyle = '#2a7a2a';
      ctx.font = '10px Courier New';
      ctx.textAlign = 'center';
      ctx.fillText(deg + '°', lx, ly);
    }
  }

  ctx.setLineDash([]);
  ctx.strokeStyle = '#2a6a2a';
  ctx.lineWidth   = 1.5;
  ctx.beginPath();
  ctx.moveTo(CX - R_MAX, CY);
  ctx.lineTo(CX + R_MAX, CY);
  ctx.stroke();

  ctx.restore();
}

function drawReturnWedge(angleDeg, dbz) {
  const color = dbzToColor(dbz);
  if (!color) return;

  const r = (dbz / 75.0) * R_MAX;
  const halfStep = 1.1 * Math.PI / 180;
  const a = (180 - angleDeg) * Math.PI / 180;

  ctx.save();
  ctx.fillStyle   = color;
  ctx.globalAlpha = 0.85;
  ctx.beginPath();
  ctx.moveTo(CX, CY);
  ctx.arc(CX, CY, r, a - halfStep, a + halfStep);
  ctx.closePath();
  ctx.fill();
  ctx.restore();
}

function drawSweepLine(angleDeg) {
  const a = (180 - angleDeg) * Math.PI / 180;
  ctx.save();
  ctx.strokeStyle = 'rgba(98, 245, 31, 0.9)';
  ctx.lineWidth   = 2;
  ctx.shadowColor = '#62f51f';
  ctx.shadowBlur  = 8;
  ctx.beginPath();
  ctx.moveTo(CX, CY);
  ctx.lineTo(CX + R_MAX * Math.cos(a), CY - R_MAX * Math.sin(a));
  ctx.stroke();
  ctx.restore();
}

function drawAircraftSymbol(pitch, roll) {
  ctx.save();
  ctx.translate(CX, CY - 16);
  ctx.rotate(roll * Math.PI / 180);
  ctx.strokeStyle = '#ffffff';
  ctx.lineWidth   = 2;

  ctx.beginPath();
  ctx.moveTo(-20, 0); ctx.lineTo(20, 0);
  ctx.moveTo(0, 0);   ctx.lineTo(0, -10);
  ctx.moveTo(-8, 6);  ctx.lineTo(8, 6);
  ctx.stroke();
  ctx.restore();
}

function render(data) {
  drawBackground();
  drawGrid();

  for (let deg = 0; deg <= 180; deg++) {
    if (sweep[deg] > 0) drawReturnWedge(deg, sweep[deg]);
  }

  drawSweepLine(scanAngle);
  drawAircraftSymbol(
    parseFloat(data.pitch || 0),
    parseFloat(data.roll  || 0)
  );

  ctx.fillStyle = 'rgba(0, 10, 0, 0.04)';
  ctx.fillRect(0, 0, W, H - 20);
}

function updateHUD(data) {
  const set = (id, val) => { document.getElementById(id).textContent = val; };
  set('hHdg',   isFinite(data.hdg)   ? parseFloat(data.hdg).toFixed(0)   : '---');
  set('hAlt',   isFinite(data.alt)   ? parseFloat(data.alt).toFixed(0)   : '---');
  set('hSpd',   isFinite(data.spd)   ? parseFloat(data.spd).toFixed(1)   : '---');
  set('hPitch', isFinite(data.pitch) ? parseFloat(data.pitch).toFixed(1) : '---');
  set('hRoll',  isFinite(data.roll)  ? parseFloat(data.roll).toFixed(1)  : '---');
  set('hLat',   data.lat !== 0       ? parseFloat(data.lat).toFixed(4)   : '---');
  set('hLon',   data.lon !== 0       ? parseFloat(data.lon).toFixed(4)   : '---');
}

let fetchActive = false;

function fetchRadarData() {
  if (fetchActive) return;
  fetchActive = true;
  fetch('/radardata')
    .then(r => r.json())
    .then(data => {

      if (Array.isArray(data.sweep)) {
        data.sweep.forEach((dbz, i) => { if (i <= 180) sweep[i] = dbz; });
      }
      scanAngle = parseInt(data.angle) || 0;
      render(data);
      updateHUD(data);
      document.getElementById('status').textContent =
        'LIVE — ' + new Date().toLocaleTimeString();
    })
    .catch(() => {
      document.getElementById('status').textContent = 'No signal — retrying…';
    })
    .finally(() => { fetchActive = false; });
}

setInterval(fetchRadarData, 50);
fetchRadarData();
</script>
</body>
</html>
)HTML";