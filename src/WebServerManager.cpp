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
    :root { --bg: #090d16; --card: rgba(18,25,43,0.9); --cyan: #00e5ff; --mag: #e040fb; --amber: #ffb300; --red: #ff1744; --green: #00e676; }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { background: var(--bg); color: #e2e8f0; font-family: -apple-system, BlinkMacSystemFont, sans-serif; display: flex; flex-direction: column; min-height: 100vh; }
    header { background: #0c111d; border-bottom: 1px solid #1e2c47; padding: 12px 20px; display: flex; justify-content: space-between; align-items: center; }
    .title { font-size: 1.1rem; font-weight: 700; background: linear-gradient(90deg, #00e5ff, #e040fb); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
    .btn { background: #1e293b; border: 1px solid #334155; color: #fff; padding: 6px 12px; border-radius: 6px; font-weight: 600; font-size: 0.8rem; cursor: pointer; }
    .btn-active { background: rgba(0, 229, 255, 0.25) !important; border-color: var(--cyan) !important; }
    #alertBanner { display: none; background: var(--red); color: #fff; text-align: center; padding: 6px; font-weight: bold; letter-spacing: 1px; }
    .main { display: grid; grid-template-columns: 1fr 380px; gap: 14px; padding: 14px; flex: 1; }
    .card { background: var(--card); border: 1px solid #1e2c47; border-radius: 8px; padding: 12px; }
    .canvas-box { position: relative; flex: 1; min-height: 440px; background: radial-gradient(circle at 50% 90%, #0d1527 0%, #080c14 80%); border-radius: 8px; }
    canvas { position: absolute; top:0; left:0; width:100%; height:100%; }
    .terminal { background: #080c14; padding: 6px; font-family: monospace; font-size: 0.72rem; color: #38bdf8; border-radius: 4px; overflow-y: auto; max-height: 60px; }
    @media (max-width: 900px) { .main { grid-template-columns: 1fr; } }
  </style>
</head>
<body>
  <div id="alertBanner">🚨 PERIMETER BREACH: INTRUDER IN ALERT ZONE 🚨</div>
  <header>
    <div>
      <div class="title">RADAR FUSION LABS</div>
      <div style="font-size:0.75rem; color:#64748b;">HLK-LD2452 (24G) & HLK-LD6003D (60G) Spatial Tracker</div>
    </div>
    <div style="display:flex; gap:6px;">
      <button class="btn" id="b0" onclick="sendCmd({cmd:'set_mode',val:0})">24G (LD2452)</button>
      <button class="btn" id="b1" onclick="sendCmd({cmd:'set_mode',val:1})">60G (LD6003D)</button>
      <button class="btn btn-active" id="b2" onclick="sendCmd({cmd:'set_mode',val:2})">DUAL FUSION</button>
    </div>
    <div id="status" style="font-family:monospace; font-size:0.8rem; color:var(--green);">● LIVE</div>
  </header>
  <div class="main">
    <div class="card" style="display:flex; flex-direction:column;">
      <div style="display:flex; justify-content:space-between; margin-bottom:8px; font-size:0.85rem; color:var(--cyan); font-weight:bold;">
        <span>SPATIAL RADAR HUD</span>
        <span id="fps">-- FPS</span>
      </div>
      <div class="canvas-box" id="cWrap"><canvas id="rCanvas"></canvas></div>
    </div>
    <div style="display:flex; flex-direction:column; gap:12px;">
      <div class="card">
        <div style="color:var(--cyan); font-weight:bold; font-size:0.85rem; margin-bottom:6px;">ACTIVE TARGETS</div>
        <div id="tgtBox" style="display:flex; flex-direction:column; gap:6px; font-size:0.8rem;">Waiting for radar...</div>
      </div>
      <div class="card">
        <div style="color:var(--cyan); font-weight:bold; font-size:0.85rem; margin-bottom:6px;">CONTROLS & GEOFENCE</div>
        <div style="display:flex; justify-content:space-between; margin-bottom:6px;">
          <span>Range:</span>
          <input type="range" min="2" max="8" step="1" id="rng" value="6" oninput="sendCmd({cmd:'set_range',val:parseFloat(this.value)})">
          <span id="rngVal">6m</span>
        </div>
        <div style="display:flex; justify-content:space-between;">
          <span>Alert Zone:</span>
          <button class="btn" onclick="sendCmd({cmd:'toggle_zone'})">Toggle Zone</button>
        </div>
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
    let ws, data = {mode:2, range:6, targets:[], alert:false}, sweep = -60, canvas, ctx;
    window.onload = () => {
      canvas = document.getElementById('rCanvas');
      ctx = canvas.getContext('2d');
      resize();
      window.onresize = resize;
      initWS();
      requestAnimationFrame(draw);
    };
    function resize() { let w = document.getElementById('cWrap'); canvas.width = w.clientWidth; canvas.height = w.clientHeight; }
    function initWS() {
      ws = new WebSocket('ws://' + window.location.host + '/ws');
      ws.onmessage = (e) => {
        try {
          data = JSON.parse(e.data);
          document.getElementById('fps').innerText = Math.round(data.fps||0) + " FPS";
          document.getElementById('alertBanner').style.display = data.alert ? 'block' : 'none';
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
      if (!tg.length) { b.innerHTML = "No targets in field"; return; }
      let h = '';
      tg.forEach(t => {
        let col = t.src===3 ? "#e040fb" : (t.src===2 ? "#ffb300" : "#00e5ff");
        h += `<div style="background:#0f172a; padding:6px; border-radius:4px; border:1px solid ${t.inZone?'#ff1744':'#1e2c47'}; font-family:monospace;">
          <strong style="color:${col}">T${t.id} [${t.src===3?'DUAL':t.src===2?'60G':'24G'}]</strong> Dist:${t.dist.toFixed(2)}m Ang:${t.ang.toFixed(1)}° Spd:${t.spd.toFixed(2)}m/s
          <div style="color:#64748b;">X:${t.x.toFixed(2)}m Y:${t.y.toFixed(2)}m | ${t.inZone?'<span style=\"color:#ff1744\">BREACH</span>':t.conf+'%'}</div>
        </div>`;
      });
      b.innerHTML = h;
    }
    function sendCmd(o) { if (ws && ws.readyState===1) ws.send(JSON.stringify(o)); }
    function draw() {
      let w = canvas.width, h = canvas.height;
      ctx.clearRect(0,0,w,h);
      let ox = w/2, oy = h - 25, maxPix = Math.min(w*0.45, h-40), maxR = data.range||6;
      ctx.beginPath(); ctx.arc(ox, oy, maxPix, -Math.PI/2 - (Math.PI/3), -Math.PI/2 + (Math.PI/3));
      ctx.strokeStyle = "#1e293b"; ctx.stroke();
      for (let r=2; r<=maxR; r+=2) {
        let pr = (r/maxR)*maxPix;
        ctx.beginPath(); ctx.arc(ox, oy, pr, -Math.PI/2 - (Math.PI/3), -Math.PI/2 + (Math.PI/3));
        ctx.strokeStyle = "#16233b"; ctx.stroke();
        ctx.fillStyle = "#64748b"; ctx.font = "10px monospace"; ctx.fillText(r+"m", ox+15, oy-pr);
      }
      sweep += 2; if (sweep > 60) sweep = -60;
      let sRad = sweep * (Math.PI/180);
      ctx.beginPath(); ctx.moveTo(ox, oy); ctx.lineTo(ox+Math.sin(sRad)*maxPix, oy-Math.cos(sRad)*maxPix);
      ctx.strokeStyle = "rgba(0,229,255,0.4)"; ctx.stroke();
      let z = data.zone;
      if (z && z.enabled) {
        let x1 = ox + (z.xMin / maxR)*maxPix, y1 = oy - (z.yMax / maxR)*maxPix;
        let x2 = ox + (z.xMax / maxR)*maxPix, y2 = oy - (z.yMin / maxR)*maxPix;
        ctx.strokeStyle = data.alert ? "#ff1744" : "#00e676";
        ctx.strokeRect(x1, y1, x2-x1, y2-y1);
      }
      (data.targets||[]).forEach(t => {
        let tx = ox + (t.x / maxR)*maxPix, ty = oy - (t.y / maxR)*maxPix;
        ctx.fillStyle = t.inZone ? "#ff1744" : (t.src===3 ? "#e040fb" : (t.src===2 ? "#ffb300" : "#00e5ff"));
        ctx.beginPath(); ctx.arc(tx, ty, 6, 0, Math.PI*2); ctx.fill();
        ctx.fillStyle = "#fff"; ctx.font = "10px monospace"; ctx.fillText(`T${t.id}:${t.dist.toFixed(1)}m`, tx+8, ty);
      });
      requestAnimationFrame(draw);
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
    setupWiFi();
    setupRoutes();

    _ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->onWebSocketEvent(server, client, type, arg, data, len);
    });

    _server.addHandler(&_ws);
    _server.begin();
}

void WebServerManager::setupWiFi() {
    WiFi.mode(WIFI_AP_STA);
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
    _ws.cleanupClients();

    uint32_t now = millis();
    // 10 Hz telemetry streaming rate
    if (now - _lastBroadcastTime >= 100) {
        _lastBroadcastTime = now;
        broadcastTelemetry();
    }
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
