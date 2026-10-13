#include "WebServerManager.h"

// Fallback embedded HTML served directly from flash memory
static const char INDEX_HTML_PROGMEM[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Radar Fusion OS // Dual 24G + 60G mmWave</title>
  <style>
    :root { --bg: #070a12; --card: rgba(14,21,37,0.9); --cyan: #00e5ff; --mag: #e040fb; --amber: #ffb300; --red: #ff1744; --green: #00e676; --text: #f1f5f9; --dim: #94a3b8; }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { background: var(--bg); color: var(--text); font-family: 'Segoe UI', -apple-system, sans-serif; display: flex; flex-direction: column; min-height: 100vh; }
    body.alert-pulse { box-shadow: inset 0 0 45px rgba(255, 23, 68, 0.7); animation: aPulse 0.7s infinite alternate; }
    @keyframes aPulse { 0% { box-shadow: inset 0 0 20px rgba(255, 23, 68, 0.4); } 100% { box-shadow: inset 0 0 65px rgba(255, 23, 68, 0.85); } }
    header { background: #0a0f1a; border-bottom: 1px solid #1a2742; padding: 10px 18px; display: flex; justify-content: space-between; align-items: center; }
    .brand-title { font-size: 1.15rem; font-weight: 800; background: linear-gradient(90deg, var(--cyan), var(--mag)); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
    .btn { background: #141e33; border: 1px solid #1a2742; color: #fff; padding: 6px 12px; border-radius: 6px; font-weight: 600; font-size: 0.8rem; cursor: pointer; transition: all 0.2s; }
    .btn:hover { background: #1e2c4a; }
    .btn-active { background: rgba(0, 229, 255, 0.2) !important; border-color: var(--cyan) !important; color: #fff !important; }
    #alertBanner { display: none; background: #ff1744; color: #fff; text-align: center; padding: 8px; font-weight: bold; letter-spacing: 2px; }
    .main { display: grid; grid-template-columns: 1fr 400px; gap: 14px; padding: 14px; flex: 1; }
    .card { background: var(--card); border: 1px solid #1a2742; border-radius: 8px; padding: 12px; }
    .canvas-box { position: relative; flex: 1; min-height: 480px; background: radial-gradient(circle at 50% 92%, #0b152b 0%, #060a14 75%); border-radius: 8px; border: 1px solid #16243d; cursor: crosshair; }
    canvas { position: absolute; top:0; left:0; width:100%; height:100%; }
    .terminal { background: #050810; border: 1px solid #121d30; padding: 6px; font-family: monospace; font-size: 0.72rem; color: #38bdf8; border-radius: 4px; overflow-y: auto; max-height: 60px; }
    .row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px; font-size: 0.82rem; }
    .input-val { background: #0b1120; border: 1px solid #1a2742; color: #fff; padding: 4px 6px; border-radius: 4px; width: 65px; font-family: monospace; }
    @media (max-width: 900px) { .main { grid-template-columns: 1fr; } }
  </style>
</head>
<body>
  <div id="alertBanner">🚨 PERIMETER BREACH: INTRUDER IN ALERT ZONE 🚨</div>
  <header>
    <div>
      <div class="brand-title">RADAR FUSION LABS</div>
      <div style="font-size:0.72rem; color:var(--dim);">Dual 24GHz (LD2452) + 60GHz (LD6003D) mmWave Console</div>
    </div>
    <div style="display:flex; gap:6px;">
      <button class="btn" id="b0" onclick="sendCmd({cmd:'set_mode',val:0})">24G (LD2452)</button>
      <button class="btn" id="b1" onclick="sendCmd({cmd:'set_mode',val:1})">60G (LD6003D)</button>
      <button class="btn btn-active" id="b2" onclick="sendCmd({cmd:'set_mode',val:2})">DUAL FUSION</button>
    </div>
    <div style="display:flex; gap:8px; align-items:center;">
      <button class="btn" id="sndBtn" onclick="toggleSnd()">🔊</button>
      <div id="status" style="font-family:monospace; font-size:0.8rem; color:var(--green);">● LIVE</div>
    </div>
  </header>
  <div class="main">
    <div class="card" style="display:flex; flex-direction:column;">
      <div style="display:flex; justify-content:space-between; margin-bottom:8px; font-size:0.85rem; color:var(--cyan); font-weight:bold;">
        <span>SPATIAL PLANAR HUD PROJECTION</span>
        <div style="display:flex; gap:6px;">
          <button class="btn" id="btnDraw" onclick="toggleDraw()">+ Draw Zone</button>
          <span id="fps" style="font-family:monospace; color:var(--dim); padding-top:4px;">-- FPS</span>
        </div>
      </div>
      <div class="canvas-box" id="cWrap"
           onmousedown="onMD(event)" onmousemove="onMM(event)" onmouseup="onMU(event)"><canvas id="rCanvas"></canvas></div>
    </div>
    <div style="display:flex; flex-direction:column; gap:12px;">
      <div class="card">
        <div style="color:var(--cyan); font-weight:bold; font-size:0.85rem; margin-bottom:6px;">ACTIVE TRACKED TARGETS</div>
        <div id="tgtBox" style="display:flex; flex-direction:column; gap:6px; font-size:0.8rem; max-height:180px; overflow-y:auto;">Awaiting radar echoes...</div>
      </div>
      <div class="card">
        <div style="color:var(--cyan); font-weight:bold; font-size:0.85rem; margin-bottom:6px;">GEOFENCE ALERT ZONE</div>
        <div class="row"><span>Active:</span><input type="checkbox" id="zEn" checked onchange="saveZone()"></div>
        <div style="display:grid; grid-template-columns:1fr 1fr; gap:6px;">
          <div class="row"><span>X Min:</span><input type="number" step="0.2" class="input-val" id="zx1" value="-1.5" onchange="saveZone()"></div>
          <div class="row"><span>X Max:</span><input type="number" step="0.2" class="input-val" id="zx2" value="1.5" onchange="saveZone()"></div>
          <div class="row"><span>Y Min:</span><input type="number" step="0.2" class="input-val" id="zy1" value="1.0" onchange="saveZone()"></div>
          <div class="row"><span>Y Max:</span><input type="number" step="0.2" class="input-val" id="zy2" value="3.5" onchange="saveZone()"></div>
        </div>
      </div>
      <div class="card">
        <div style="color:var(--cyan); font-weight:bold; font-size:0.85rem; margin-bottom:6px;">CONTROLS & RANGE</div>
        <div class="row">
          <span>Max Range:</span>
          <input type="range" min="2" max="8" step="1" id="rng" value="6" oninput="sendCmd({cmd:'set_range',val:parseFloat(this.value)})">
          <span id="rngVal" class="input-val" style="width:45px;">6m</span>
        </div>
        <div class="row"><span>Cartesian Grid:</span><input type="checkbox" id="grdChk" checked></div>
      </div>
      <div class="card">
        <div style="color:var(--cyan); font-weight:bold; font-size:0.85rem; margin-bottom:4px;">RAW UART SNIFFER</div>
        <div style="font-size:0.7rem; color:var(--cyan);">HLK-LD2452 (24G):</div>
        <div class="terminal" id="raw24">--</div>
        <div style="font-size:0.7rem; color:var(--amber); margin-top:4px;">HLK-LD6003D (60G):</div>
        <div class="terminal" id="raw60">--</div>
      </div>
    </div>
  </div>
  <script>
    let ws, data = {mode:2, range:6, targets:[], alert:false, zone:{enabled:true,xMin:-1.5,xMax:1.5,yMin:1.0,yMax:3.5}};
    let sweep = -60, sDir = 1, canvas, ctx, isDraw = false, pStart = null, pCur = null, audioCtx = null, mute = false, lastSnd = 0;

    function playBeep() {
      if (mute) return;
      let now = Date.now();
      if (now - lastSnd < 350) return;
      lastSnd = now;
      try {
        if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        let osc = audioCtx.createOscillator(), g = audioCtx.createGain();
        osc.type = 'sawtooth';
        osc.frequency.setValueAtTime(880, audioCtx.currentTime);
        osc.frequency.exponentialRampToValueAtTime(440, audioCtx.currentTime + 0.2);
        g.gain.setValueAtTime(0.18, audioCtx.currentTime);
        g.gain.linearRampToValueAtTime(0.01, audioCtx.currentTime + 0.2);
        osc.connect(g); g.connect(audioCtx.destination);
        osc.start(); osc.stop(audioCtx.currentTime + 0.2);
      } catch(e){}
    }
    function toggleSnd() { mute = !mute; document.getElementById('sndBtn').innerText = mute ? '🔇' : '🔊'; }

    window.onload = () => {
      canvas = document.getElementById('rCanvas'); ctx = canvas.getContext('2d');
      resize(); window.onresize = resize; initWS(); requestAnimationFrame(render);
    };
    function resize() { let w = document.getElementById('cWrap'); canvas.width = w.clientWidth; canvas.height = w.clientHeight; }
    function initWS() {
      ws = new WebSocket('ws://' + window.location.host + '/ws');
      ws.onmessage = (e) => {
        try {
          data = JSON.parse(e.data);
          document.getElementById('fps').innerText = Math.round(data.fps||0) + " FPS";
          document.getElementById('alertBanner').style.display = data.alert ? 'block' : 'none';
          document.body.className = data.alert ? 'alert-pulse' : '';
          if (data.alert) playBeep();
          document.getElementById('rngVal').innerText = (data.range||6) + "m";
          document.getElementById('rng').value = data.range||6;
          ['b0','b1','b2'].forEach((id, i) => document.getElementById(id).className = (data.mode===i) ? "btn btn-active" : "btn");
          if(data.raw24) document.getElementById('raw24').innerText = data.raw24;
          if(data.raw60) document.getElementById('raw60').innerText = data.raw60;
          renderTgt(data.targets||[]);
        } catch(err){}
      };
      ws.onclose = () => { setTimeout(initWS, 2000); };
    }
    function renderTgt(tg) {
      let b = document.getElementById('tgtBox');
      if (!tg.length) { b.innerHTML = "<div style='color:#64748b;'>No echoes detected</div>"; return; }
      let h = '';
      tg.forEach(t => {
        let col = t.src===3 ? "#e040fb" : (t.src===2 ? "#ffb300" : "#00e5ff");
        h += `<div style="background:#0a1120; padding:6px; border-radius:4px; border:1px solid ${t.inZone?'#ff1744':'#1a2742'}; font-family:monospace;">
          <strong style="color:${col}">T${t.id} [${t.src===3?'DUAL':t.src===2?'60G':'24G'}]</strong> Dist:${t.dist.toFixed(2)}m Ang:${t.ang.toFixed(1)}° Spd:${t.spd.toFixed(2)}m/s
          <div style="color:#64748b;">X:${t.x.toFixed(2)}m Y:${t.y.toFixed(2)}m | ${t.inZone?'<span style=\"color:#ff1744\">BREACH</span>':t.conf+'%'}</div>
        </div>`;
      });
      b.innerHTML = h;
    }
    function sendCmd(o) { if (ws && ws.readyState===1) ws.send(JSON.stringify(o)); }
    function saveZone() {
      sendCmd({
        cmd: "set_zone",
        enabled: document.getElementById('zEn').checked,
        xMin: parseFloat(document.getElementById('zx1').value),
        xMax: parseFloat(document.getElementById('zx2').value),
        yMin: parseFloat(document.getElementById('zy1').value),
        yMax: parseFloat(document.getElementById('zy2').value)
      });
    }
    function toggleDraw() {
      isDraw = !isDraw;
      let b = document.getElementById('btnDraw');
      b.className = isDraw ? "btn btn-active" : "btn";
      b.innerText = isDraw ? "Drag on Map..." : "+ Draw Zone";
    }
    function getPos(e) { let r = canvas.getBoundingClientRect(); return { x: e.clientX - r.left, y: e.clientY - r.top }; }
    function screenToWorld(px, py) {
      let ox = canvas.width / 2, oy = canvas.height - 30, maxPix = Math.min(canvas.width * 0.44, canvas.height - 50), maxR = data.range || 6;
      return { x: ((px - ox) / maxPix) * maxR, y: ((oy - py) / maxPix) * maxR };
    }
    function onMD(e) { if (isDraw) { pStart = getPos(e); pCur = pStart; } }
    function onMM(e) { if (isDraw && pStart) pCur = getPos(e); }
    function onMU(e) {
      if (isDraw && pStart && pCur) {
        let w1 = screenToWorld(pStart.x, pStart.y), w2 = screenToWorld(pCur.x, pCur.y);
        document.getElementById('zx1').value = Math.min(w1.x, w2.x).toFixed(2);
        document.getElementById('zx2').value = Math.max(w1.x, w2.x).toFixed(2);
        document.getElementById('zy1').value = Math.min(w1.y, w2.y).toFixed(2);
        document.getElementById('zy2').value = Math.max(w1.y, w2.y).toFixed(2);
        saveZone(); toggleDraw(); pStart = null; pCur = null;
      }
    }
    function render() {
      let w = canvas.width, h = canvas.height; ctx.clearRect(0,0,w,h);
      let ox = w/2, oy = h - 30, maxPix = Math.min(w*0.44, h-50), maxR = data.range||6;

      ctx.beginPath(); ctx.arc(ox, oy, maxPix, -Math.PI/2 - (Math.PI/3), -Math.PI/2 + (Math.PI/3));
      ctx.strokeStyle = "#16233b"; ctx.lineWidth = 1.5; ctx.stroke();

      for (let r=2; r<=maxR; r+=2) {
        let pr = (r/maxR)*maxPix;
        ctx.beginPath(); ctx.arc(ox, oy, pr, -Math.PI/2 - (Math.PI/3), -Math.PI/2 + (Math.PI/3));
        ctx.strokeStyle = "#132036"; ctx.stroke();
        ctx.fillStyle = "#64748b"; ctx.font = "bold 11px monospace"; ctx.fillText(r+"m", ox+15, oy-pr);
      }
      sweep += 1.8 * sDir;
      if (sweep > 60) { sweep = 60; sDir = -1; }
      else if (sweep < -60) { sweep = -60; sDir = 1; }
      let sRad = sweep * (Math.PI/180);
      ctx.beginPath(); ctx.moveTo(ox, oy); ctx.lineTo(ox+Math.sin(sRad)*maxPix, oy-Math.cos(sRad)*maxPix);
      ctx.strokeStyle = "rgba(0,229,255,0.75)"; ctx.lineWidth = 2; ctx.stroke();

      let z = data.zone;
      if (z && z.enabled) {
        let x1 = ox + (z.xMin / maxR)*maxPix, y1 = oy - (z.yMax / maxR)*maxPix;
        let x2 = ox + (z.xMax / maxR)*maxPix, y2 = oy - (z.yMin / maxR)*maxPix;
        ctx.fillStyle = data.alert ? "rgba(255,23,68,0.15)" : "rgba(0,230,118,0.08)";
        ctx.fillRect(x1, y1, x2-x1, y2-y1);
        ctx.strokeStyle = data.alert ? "#ff1744" : "#00e676";
        ctx.setLineDash([6, 4]); ctx.strokeRect(x1, y1, x2-x1, y2-y1); ctx.setLineDash([]);
      }
      if (isDraw && pStart && pCur) {
        ctx.strokeStyle = "#00e5ff"; ctx.lineWidth = 2;
        ctx.strokeRect(Math.min(pStart.x, pCur.x), Math.min(pStart.y, pCur.y), Math.abs(pStart.x - pCur.x), Math.abs(pStart.y - pCur.y));
      }
      (data.targets||[]).forEach(t => {
        let tx = ox + (t.x / maxR)*maxPix, ty = oy - (t.y / maxR)*maxPix;
        let col = t.inZone ? "#ff1744" : (t.src===3 ? "#e040fb" : (t.src===2 ? "#ffb300" : "#00e5ff"));
        ctx.beginPath(); ctx.arc(tx, ty, 6, 0, Math.PI*2); ctx.fillStyle = col; ctx.fill();
        ctx.strokeStyle = "#fff"; ctx.lineWidth = 1.5; ctx.stroke();
        ctx.fillStyle = "#fff"; ctx.font = "bold 11px monospace"; ctx.fillText(`T${t.id}:${t.dist.toFixed(1)}m`, tx+9, ty);
      });
      requestAnimationFrame(render);
    }
  </script>
</body>
</html>
)rawliteral";

WebServerManager::WebServerManager(SensorFusionEngine& fusion, SystemConfig& config,
                                   LD2452Driver& radar24, LD6003DDriver& radar60)
    : _server(WEB_SERVER_PORT), _ws(WS_PATH), _fusion(fusion), _config(config),
      _radar24(radar24), _radar60(radar60), _ipAddress(""), _connectedSTA(false),
      _lastBroadcastTime(0) {}

void WebServerManager::begin() {
#if ENABLE_WIFI
    setupWiFi();
    setupRoutes();

    _ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->onWebSocketEvent(server, client, type, arg, data, len);
    });

    _server.addHandler(&_ws);
    _server.begin();
#else
    _connectedSTA = false;
    _ipAddress = "USB Mode (WiFi Off)";
#endif
}

void WebServerManager::setupWiFi() {
    // If SSID is placeholder, immediately boot SoftAP hotspot without 6-second blocking delay
    if (strcmp(DEFAULT_WIFI_SSID, "YourWiFiSSID") == 0) {
        _connectedSTA = false;
        WiFi.mode(WIFI_AP);
        WiFi.setTxPower(WIFI_POWER_8_5dBm); // Reduce RF power spike
        WiFi.softAP(AP_SSID, AP_PASS);
        _ipAddress = WiFi.softAPIP().toString();
        return;
    }

    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm); // Reduce RF power spike
    WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);

    uint32_t startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 6000) {
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        _connectedSTA = true;
        _ipAddress = WiFi.localIP().toString();
    } else {
        // Start SoftAP as fallback hotspot
        _connectedSTA = false;
        WiFi.softAP(AP_SSID, AP_PASS);
        _ipAddress = WiFi.softAPIP().toString();
    }
}

void WebServerManager::setupRoutes() {
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", INDEX_HTML_PROGMEM);
    });
}

void WebServerManager::onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                        AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        // Broadcast immediate initial frame to new client
        broadcastTelemetry();
    } else if (type == WS_EVT_DATA) {
        handleWebSocketMessage(arg, data, len);
    }
}

void WebServerManager::handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, (char*)data);
        if (err) return;

        const char* cmd = doc["cmd"];
        if (!cmd) return;

        if (strcmp(cmd, "set_mode") == 0) {
            uint8_t mode = doc["val"];
            if (mode <= 2) _config.activeMode = (SensorMode)mode;
        } else if (strcmp(cmd, "set_range") == 0) {
            float r = doc["val"];
            if (r >= 2.0f && r <= 8.0f) _config.maxRange = r;
        } else if (strcmp(cmd, "set_track") == 0) {
            _config.individualTrackId = doc["val"];
        } else if (strcmp(cmd, "toggle_grid") == 0) {
            _config.gridLines = !_config.gridLines;
        } else if (strcmp(cmd, "toggle_zone") == 0) {
            _config.zone.enabled = !_config.zone.enabled;
        } else if (strcmp(cmd, "set_zone") == 0) {
            _config.zone.enabled = doc["enabled"];
            _config.zone.xMin = doc["xMin"];
            _config.zone.xMax = doc["xMax"];
            _config.zone.yMin = doc["yMin"];
            _config.zone.yMax = doc["yMax"];
        }
    }
}

void WebServerManager::update() {
#if ENABLE_WIFI
    _ws.cleanupClients();

    uint32_t now = millis();
    // 10 Hz telemetry streaming rate
    if (now - _lastBroadcastTime >= 100) {
        _lastBroadcastTime = now;
        broadcastTelemetry();
    }
#endif
}

void WebServerManager::broadcastTelemetry() {
    if (_ws.count() == 0) return; // No connected web clients, skip serialization

    StaticJsonDocument<2048> doc;
    doc["mode"] = (uint8_t)_config.activeMode;
    doc["range"] = _config.maxRange;
    doc["grid"] = _config.gridLines;
    doc["fps"] = _config.fps;
    doc["alert"] = _config.alertActive;
    doc["ip"] = _ipAddress;

    // Alert zone
    JsonObject zoneObj = doc.createNestedObject("zone");
    zoneObj["enabled"] = _config.zone.enabled;
    zoneObj["xMin"] = _config.zone.xMin;
    zoneObj["xMax"] = _config.zone.xMax;
    zoneObj["yMin"] = _config.zone.yMin;
    zoneObj["yMax"] = _config.zone.yMax;

    // Raw Hex
    doc["raw24"] = _radar24.getLatestRawHex();
    doc["raw60"] = _radar60.getLatestRawHex();

    // Targets Array
    JsonArray targetsArr = doc.createNestedArray("targets");
    uint8_t count = _fusion.getTargetCount();

    for (uint8_t i = 0; i < count; i++) {
        const Target& t = _fusion.getTarget(i);
        if (!t.valid) continue;

        JsonObject tObj = targetsArr.createNestedObject();
        tObj["id"] = t.id;
        tObj["x"] = t.x;
        tObj["y"] = t.y;
        tObj["dist"] = t.distance;
        tObj["ang"] = t.angle;
        tObj["spd"] = t.speed;
        tObj["src"] = t.sensorSource;
        tObj["conf"] = t.confidence;
        tObj["inZone"] = t.inAlertZone;

        JsonArray trailArr = tObj.createNestedArray("trail");
        for (int k = 0; k < t.trailCount; k++) {
            JsonObject pt = trailArr.createNestedObject();
            pt["x"] = t.trail[k].x;
            pt["y"] = t.trail[k].y;
        }
    }

    String jsonOutput;
    serializeJson(doc, jsonOutput);
    _ws.textAll(jsonOutput);
}
