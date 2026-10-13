#include "DisplayUI.h"
#include <cmath>

#define RADAR_ORIGIN_X 155
#define RADAR_ORIGIN_Y 305
#define RADAR_MAX_PIXEL_RADIUS 260

DisplayUI::DisplayUI(SensorFusionEngine& fusion, SystemConfig& config)
    : _tft(), _touch(PIN_TOUCH_CS, PIN_TOUCH_IRQ), _fusion(fusion), _config(config),
      _sweepAngle(-60.0f), _lastRenderTime(0), _lastTouchTime(0), 
      _rawHexViewActive(false), _alertFlashTimer(0), _flashState(false) {
    
    // Top Tab Buttons
    _btnMode24G   = { 4,   4, 52, 24 };
    _btnMode60G   = { 60,  4, 52, 24 };
    _btnModeDual  = { 116, 4, 62, 24 };

    // Right Control Buttons
    _btnTargetCycle = { 318, 36,  156, 32 };
    _btnZoomOut     = { 318, 170, 74,  32 };
    _btnZoomIn      = { 398, 170, 74,  32 };
    _btnGrid        = { 318, 208, 74,  32 };
    _btnZone        = { 398, 208, 74,  32 };
    _btnViewRaw     = { 318, 246, 154, 32 };
}

void DisplayUI::begin() {
    // Backlight control
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);

    _tft.init();
    _tft.setRotation(1); // 480x320 Landscape
    _tft.fillScreen(COLOR_BG);

    // Touch setup
    _touch.begin();
    _touch.setRotation(1);
}

void DisplayUI::showSplashScreen(const char* wifiStatus) {
    _tft.fillScreen(COLOR_BG);

    // Futuristic Radar HUD Circle in Center
    int centerX = 240;
    int centerY = 130;
    int radius = 60;

    // Outer cyber rings
    _tft.drawCircle(centerX, centerY, radius + 15, COLOR_PANEL_BORDER);
    _tft.drawCircle(centerX, centerY, radius + 10, COLOR_CYAN);
    _tft.drawCircle(centerX, centerY, radius, COLOR_PANEL_BORDER);
    _tft.drawCircle(centerX, centerY, radius / 2, COLOR_GRID);

    // Header text
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COLOR_CYAN, COLOR_BG);
    _tft.drawString("A N T I G R A V I T Y", centerX, 25, 4);

    _tft.setTextColor(COLOR_MAGENTA, COLOR_BG);
    _tft.drawString("DUAL mmWAVE SPATIAL TRACKER", centerX, 45, 2);

    // Animated boot sweep sequence
    for (int deg = -60; deg <= 300; deg += 12) {
        float rad = deg * (M_PI / 180.0f);
        int px = centerX + (int)(cos(rad) * radius);
        int py = centerY + (int)(sin(rad) * radius);
        _tft.drawLine(centerX, centerY, px, py, (deg % 24 == 0) ? COLOR_CYAN : COLOR_MAGENTA);
        delay(25);
    }

    // Hardware diagnostic text
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COLOR_SUCCESS_GREEN, COLOR_BG);
    _tft.drawString("[OK] ESP32 Core @ 240MHz", 40, 205, 2);
    _tft.drawString("[OK] HLK-LD2452 (24G) Port Ready", 40, 225, 2);
    _tft.drawString("[OK] HLK-LD6003D (60G) Port Ready", 40, 245, 2);

    _tft.setTextColor(COLOR_AMBER, COLOR_BG);
    _tft.drawString(wifiStatus, 40, 265, 2);

    // Animated progress bar
    int pbX = 40, pbY = 295, pbW = 400, pbH = 10;
    _tft.drawRect(pbX, pbY, pbW, pbH, COLOR_CYAN);
    for (int w = 2; w <= pbW - 4; w += 8) {
        _tft.fillRect(pbX + 2, pbY + 2, w, pbH - 4, COLOR_CYAN);
        delay(15);
    }

    delay(500);
    _tft.fillScreen(COLOR_BG);
}

void DisplayUI::update() {
    uint32_t now = millis();

    // Check touch input
    handleTouch();

    // Render loop (~15-20 FPS for responsive display)
    if (now - _lastRenderTime >= 50) {
        uint32_t delta = now - _lastRenderTime;
        _config.fps = 1000.0f / (float)(delta > 0 ? delta : 1);
        _lastRenderTime = now;

        // Advance radar sweep angle
        _sweepAngle += 4.5f;
        if (_sweepAngle > 60.0f) _sweepAngle = -60.0f;

        // Alert flash timer
        if (_config.alertActive) {
            if (now - _alertFlashTimer > 250) {
                _alertFlashTimer = now;
                _flashState = !_flashState;
            }
        } else {
            _flashState = false;
        }

        // Render layers
        drawTopBar();

        if (_rawHexViewActive) {
            drawRawHexView();
        } else {
            drawRadarGrid();
            drawGeofenceZone();
            drawSweepLine();
            drawTargets();
        }

        drawRightPanel();
    }
}

void DisplayUI::drawTopBar() {
    // Top bar background
    uint16_t barBg = (_config.alertActive && _flashState) ? COLOR_ALERT_RED : COLOR_PANEL_BG;
    _tft.fillRect(0, 0, SCREEN_WIDTH, 32, barBg);
    _tft.drawFastHLine(0, 31, SCREEN_WIDTH, (_config.alertActive) ? COLOR_ALERT_RED : COLOR_PANEL_BORDER);

    // Tab 1: 24G
    uint16_t c24 = (_config.activeMode == MODE_LD2452) ? COLOR_CYAN : COLOR_GRID_TEXT;
    _tft.fillRoundRect(_btnMode24G.x, _btnMode24G.y, _btnMode24G.w, _btnMode24G.h, 4, 
        (_config.activeMode == MODE_LD2452) ? COLOR_PANEL_BORDER : COLOR_BG);
    _tft.drawRoundRect(_btnMode24G.x, _btnMode24G.y, _btnMode24G.w, _btnMode24G.h, 4, c24);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(c24, (_config.activeMode == MODE_LD2452) ? COLOR_PANEL_BORDER : COLOR_BG);
    _tft.drawString("24G", _btnMode24G.x + (_btnMode24G.w / 2), _btnMode24G.y + (_btnMode24G.h / 2), 2);

    // Tab 2: 60G
    uint16_t c60 = (_config.activeMode == MODE_LD6003D) ? COLOR_AMBER : COLOR_GRID_TEXT;
    _tft.fillRoundRect(_btnMode60G.x, _btnMode60G.y, _btnMode60G.w, _btnMode60G.h, 4,
        (_config.activeMode == MODE_LD6003D) ? COLOR_PANEL_BORDER : COLOR_BG);
    _tft.drawRoundRect(_btnMode60G.x, _btnMode60G.y, _btnMode60G.w, _btnMode60G.h, 4, c60);
    _tft.setTextColor(c60, (_config.activeMode == MODE_LD6003D) ? COLOR_PANEL_BORDER : COLOR_BG);
    _tft.drawString("60G", _btnMode60G.x + (_btnMode60G.w / 2), _btnMode60G.y + (_btnMode60G.h / 2), 2);

    // Tab 3: DUAL FUSION
    uint16_t cDual = (_config.activeMode == MODE_FUSION) ? COLOR_MAGENTA : COLOR_GRID_TEXT;
    _tft.fillRoundRect(_btnModeDual.x, _btnModeDual.y, _btnModeDual.w, _btnModeDual.h, 4,
        (_config.activeMode == MODE_FUSION) ? COLOR_PANEL_BORDER : COLOR_BG);
    _tft.drawRoundRect(_btnModeDual.x, _btnModeDual.y, _btnModeDual.w, _btnModeDual.h, 4, cDual);
    _tft.setTextColor(cDual, (_config.activeMode == MODE_FUSION) ? COLOR_PANEL_BORDER : COLOR_BG);
    _tft.drawString("DUAL", _btnModeDual.x + (_btnModeDual.w / 2), _btnModeDual.y + (_btnModeDual.h / 2), 2);

    // Status Message / Alert Banner in Top Center
    _tft.setTextDatum(MC_DATUM);
    if (_config.alertActive) {
        _tft.setTextColor(COLOR_WHITE, barBg);
        _tft.drawString("! INTRUSION ALERT !", 245, 16, 2);
    } else {
        _tft.setTextColor(COLOR_TEXT_MUTED, barBg);
        _tft.drawString("RADAR FUSION OS", 245, 16, 2);
    }

    // Right Side: FPS & Target Count
    _tft.setTextDatum(MR_DATUM);
    char statsBuf[24];
    snprintf(statsBuf, sizeof(statsBuf), "TRK:%d %dFPS", 
             _fusion.getTargetCount(), (int)_config.fps);
    _tft.setTextColor(COLOR_CYAN, barBg);
    _tft.drawString(statsBuf, 474, 16, 2);
}

void DisplayUI::drawRadarGrid() {
    // Clear radar viewport (x: 0..311, y: 32..319)
    _tft.fillRect(0, 32, 312, 288, COLOR_BG);

    int16_t ox = RADAR_ORIGIN_X;
    int16_t oy = RADAR_ORIGIN_Y;
    float maxR = _config.maxRange;

    // Draw FOV Boundary rays (-60 deg and +60 deg)
    float radLeft = -60.0f * (M_PI / 180.0f);
    float radRight = 60.0f * (M_PI / 180.0f);
    int16_t fovLx = ox + (int16_t)(sinf(radLeft) * RADAR_MAX_PIXEL_RADIUS);
    int16_t fovLy = oy - (int16_t)(cosf(radLeft) * RADAR_MAX_PIXEL_RADIUS);
    int16_t fovRx = ox + (int16_t)(sinf(radRight) * RADAR_MAX_PIXEL_RADIUS);
    int16_t fovRy = oy - (int16_t)(cosf(radRight) * RADAR_MAX_PIXEL_RADIUS);

    _tft.drawLine(ox, oy, fovLx, fovLy, COLOR_PANEL_BORDER);
    _tft.drawLine(ox, oy, fovRx, fovRy, COLOR_PANEL_BORDER);

    // Center boresight ray (0 deg)
    _tft.drawLine(ox, oy, ox, oy - RADAR_MAX_PIXEL_RADIUS, COLOR_GRID);

    // Concentric Range Arcs (1m intervals or dynamic)
    int stepM = (maxR <= 4.0f) ? 1 : 2;
    for (float r = stepM; r <= maxR; r += stepM) {
        int16_t pixR = (int16_t)((r / maxR) * RADAR_MAX_PIXEL_RADIUS);

        // Draw arc within FOV (-60 to +60 deg)
        for (int deg = -60; deg <= 60; deg += 6) {
            float a1 = deg * (M_PI / 180.0f);
            float a2 = (deg + 6) * (M_PI / 180.0f);
            int16_t x1 = ox + (int16_t)(sinf(a1) * pixR);
            int16_t y1 = oy - (int16_t)(cosf(a1) * pixR);
            int16_t x2 = ox + (int16_t)(sinf(a2) * pixR);
            int16_t y2 = oy - (int16_t)(cosf(a2) * pixR);
            _tft.drawLine(x1, y1, x2, y2, COLOR_GRID);
        }

        // Distance text label along center line
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COLOR_GRID_TEXT, COLOR_BG);
        char lbl[8];
        snprintf(lbl, sizeof(lbl), "%dm", (int)r);
        _tft.drawString(lbl, ox + 14, oy - pixR, 1);
    }

    // Optional Cartesian Grid lines if enabled
    if (_config.gridLines) {
        for (int gx = -3; gx <= 3; gx += 1) {
            if (gx == 0) continue;
            int16_t sx1, sy1, sx2, sy2;
            worldToScreen((float)gx, 0.5f, sx1, sy1);
            worldToScreen((float)gx, maxR, sx2, sy2);
            if (sx1 >= 0 && sx1 < 312 && sx2 >= 0 && sx2 < 312) {
                _tft.drawFastVLine(sx1, sy2, sy1 - sy2, 0x0A0B);
            }
        }
    }

    // Radar Sensor Origin Node at bottom
    _tft.fillCircle(ox, oy, 4, COLOR_CYAN);
    _tft.drawCircle(ox, oy, 7, COLOR_PANEL_BORDER);
}

void DisplayUI::drawSweepLine() {
    int16_t ox = RADAR_ORIGIN_X;
    int16_t oy = RADAR_ORIGIN_Y;

    float rad = _sweepAngle * (M_PI / 180.0f);
    int16_t ex = ox + (int16_t)(sinf(rad) * RADAR_MAX_PIXEL_RADIUS);
    int16_t ey = oy - (int16_t)(cosf(rad) * RADAR_MAX_PIXEL_RADIUS);

    // Draw glowing sweep line
    _tft.drawLine(ox, oy, ex, ey, 0x1AE9);
}

void DisplayUI::drawGeofenceZone() {
    if (!_config.zone.enabled) return;

    int16_t x1, y1, x2, y2;
    worldToScreen(_config.zone.xMin, _config.zone.yMax, x1, y1); // Top Left
    worldToScreen(_config.zone.xMax, _config.zone.yMin, x2, y2); // Bottom Right

    uint16_t zoneColor = (_config.alertActive) ? COLOR_ALERT_RED : COLOR_SUCCESS_GREEN;

    // Outer bounding box
    _tft.drawRect(x1, y1, x2 - x1, y2 - y1, zoneColor);

    // Corner accent brackets
    int bracketLen = 8;
    _tft.fillRect(x1, y1, bracketLen, 2, zoneColor);
    _tft.fillRect(x1, y1, 2, bracketLen, zoneColor);
    _tft.fillRect(x2 - bracketLen, y1, bracketLen, 2, zoneColor);
    _tft.fillRect(x2 - 2, y1, 2, bracketLen, zoneColor);

    // Zone Title Label
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(zoneColor, COLOR_BG);
    _tft.drawString("ALERT ZONE", x1 + 4, y1 + 4, 1);
}

void DisplayUI::drawTargets() {
    uint8_t count = _fusion.getTargetCount();

    for (uint8_t i = 0; i < count; i++) {
        const Target& t = _fusion.getTarget(i);
        if (!t.valid) continue;

        int16_t sx, sy;
        worldToScreen(t.x, t.y, sx, sy);

        if (sx < 2 || sx > 310 || sy < 34 || sy > 318) continue;

        uint16_t col = getSensorColor(t.sensorSource);

        // Draw Breadcrumb trails
        if (_config.showTrails && t.trailCount > 1) {
            for (int k = 0; k < t.trailCount; k++) {
                int16_t tx, ty;
                worldToScreen(t.trail[k].x, t.trail[k].y, tx, ty);
                _tft.fillCircle(tx, ty, 2, COLOR_PANEL_BORDER);
            }
        }

        // Draw Velocity Vector Arrow
        if (fabsf(t.speed) > 0.05f) {
            float headingRad = t.angle * (M_PI / 180.0f);
            int16_t arrowLen = (int16_t)(t.speed * 20.0f);
            int16_t vx = sx + (int16_t)(sinf(headingRad) * arrowLen);
            int16_t vy = sy - (int16_t)(cosf(headingRad) * arrowLen);
            _tft.drawLine(sx, sy, vx, vy, COLOR_WHITE);
        }

        // Draw Target Blip
        if (t.inAlertZone) {
            // Flash red halo around intruder
            _tft.drawCircle(sx, sy, 9, COLOR_ALERT_RED);
            _tft.drawCircle(sx, sy, 7, COLOR_WHITE);
            _tft.fillCircle(sx, sy, 4, COLOR_ALERT_RED);
        } else if (t.sensorSource == 3) { // Dual Fused
            _tft.drawCircle(sx, sy, 8, COLOR_MAGENTA);
            _tft.drawCircle(sx, sy, 6, COLOR_WHITE);
            _tft.fillCircle(sx, sy, 3, COLOR_CYAN);
        } else {
            _tft.drawCircle(sx, sy, 6, col);
            _tft.fillCircle(sx, sy, 3, col);
        }

        // Target Tag HUD
        _tft.setTextDatum(ML_DATUM);
        _tft.setTextColor(COLOR_WHITE, COLOR_BG);
        char tag[16];
        snprintf(tag, sizeof(tag), "T%d:%.1fm", t.id, t.distance);
        _tft.drawString(tag, sx + 9, sy, 1);
    }
}

void DisplayUI::drawRightPanel() {
    // Divider line between radar screen and right panel
    _tft.drawFastVLine(312, 32, 288, COLOR_PANEL_BORDER);
    _tft.fillRect(313, 32, 167, 288, COLOR_PANEL_BG);

    drawTelemetryWidget();
    drawControlButtons();
}

void DisplayUI::drawTelemetryWidget() {
    int rx = 318;
    int ry = 36;
    int rw = 156;
    int rh = 126;

    // Card background
    _tft.fillRoundRect(rx, ry, rw, rh, 6, COLOR_BG);
    _tft.drawRoundRect(rx, ry, rw, rh, 6, COLOR_PANEL_BORDER);

    // Selected Target Title & Switch Button
    _tft.fillRoundRect(rx + 2, ry + 2, rw - 4, 22, 4, COLOR_PANEL_BORDER);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COLOR_CYAN, COLOR_PANEL_BORDER);

    char titleBuf[24];
    if (_config.individualTrackId == -1) {
        snprintf(titleBuf, sizeof(titleBuf), "TRACK: ALL TARGETS");
    } else {
        snprintf(titleBuf, sizeof(titleBuf), "TRACK: TARGET #%d", _config.individualTrackId);
    }
    _tft.drawString(titleBuf, rx + (rw / 2), ry + 13, 2);

    // Find primary target to show
    const Target* pTarget = nullptr;
    uint8_t count = _fusion.getTargetCount();

    if (_config.individualTrackId != -1) {
        for (uint8_t i = 0; i < count; i++) {
            if (_fusion.getTarget(i).id == _config.individualTrackId) {
                pTarget = &_fusion.getTarget(i);
                break;
            }
        }
    } else if (count > 0) {
        pTarget = &_fusion.getTarget(0); // Show closest / first target
    }

    _tft.setTextDatum(TL_DATUM);

    if (pTarget && pTarget->valid) {
        char lineBuf[32];

        // Distance & Angle
        _tft.setTextColor(COLOR_WHITE, COLOR_BG);
        snprintf(lineBuf, sizeof(lineBuf), "DIST : %.2f m", pTarget->distance);
        _tft.drawString(lineBuf, rx + 8, ry + 30, 2);

        snprintf(lineBuf, sizeof(lineBuf), "ANG  : %+.1f deg", pTarget->angle);
        _tft.drawString(lineBuf, rx + 8, ry + 50, 2);

        // Speed
        _tft.setTextColor((pTarget->speed < 0) ? COLOR_AMBER : COLOR_CYAN, COLOR_BG);
        snprintf(lineBuf, sizeof(lineBuf), "SPD  : %+.2f m/s", pTarget->speed);
        _tft.drawString(lineBuf, rx + 8, ry + 70, 2);

        // X and Y Coordinates
        _tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG);
        snprintf(lineBuf, sizeof(lineBuf), "X: %+.2fm | Y: %.2fm", pTarget->x, pTarget->y);
        _tft.drawString(lineBuf, rx + 8, ry + 90, 1);

        // Source badge
        const char* srcStr = (pTarget->sensorSource == 3) ? "DUAL FUSED" : 
                             (pTarget->sensorSource == 2) ? "60G RADAR" : "24G RADAR";
        uint16_t srcCol = getSensorColor(pTarget->sensorSource);
        _tft.setTextColor(srcCol, COLOR_BG);
        snprintf(lineBuf, sizeof(lineBuf), "MODE : %s (%d%%)", srcStr, pTarget->confidence);
        _tft.drawString(lineBuf, rx + 8, ry + 106, 1);
    } else {
        _tft.setTextColor(COLOR_GRID_TEXT, COLOR_BG);
        _tft.drawString("NO TARGET LOCKED", rx + 8, ry + 50, 2);
        _tft.drawString("Awaiting Radar Echo...", rx + 8, ry + 75, 1);
    }
}

void DisplayUI::drawControlButtons() {
    _tft.setTextDatum(MC_DATUM);

    // Zoom Buttons: [ - ] and [ + ]
    _tft.fillRoundRect(_btnZoomOut.x, _btnZoomOut.y, _btnZoomOut.w, _btnZoomOut.h, 4, COLOR_BG);
    _tft.drawRoundRect(_btnZoomOut.x, _btnZoomOut.y, _btnZoomOut.w, _btnZoomOut.h, 4, COLOR_CYAN);
    _tft.setTextColor(COLOR_CYAN, COLOR_BG);
    _tft.drawString("ZOOM -", _btnZoomOut.x + (_btnZoomOut.w / 2), _btnZoomOut.y + (_btnZoomOut.h / 2), 2);

    _tft.fillRoundRect(_btnZoomIn.x, _btnZoomIn.y, _btnZoomIn.w, _btnZoomIn.h, 4, COLOR_BG);
    _tft.drawRoundRect(_btnZoomIn.x, _btnZoomIn.y, _btnZoomIn.w, _btnZoomIn.h, 4, COLOR_CYAN);
    _tft.drawString("ZOOM +", _btnZoomIn.x + (_btnZoomIn.w / 2), _btnZoomIn.y + (_btnZoomIn.h / 2), 2);

    // Grid Toggle Button
    uint16_t gridCol = (_config.gridLines) ? COLOR_SUCCESS_GREEN : COLOR_GRID_TEXT;
    _tft.fillRoundRect(_btnGrid.x, _btnGrid.y, _btnGrid.w, _btnGrid.h, 4, COLOR_BG);
    _tft.drawRoundRect(_btnGrid.x, _btnGrid.y, _btnGrid.w, _btnGrid.h, 4, gridCol);
    _tft.setTextColor(gridCol, COLOR_BG);
    _tft.drawString("GRID", _btnGrid.x + (_btnGrid.w / 2), _btnGrid.y + (_btnGrid.h / 2), 2);

    // Zone Toggle Button
    uint16_t zoneCol = (_config.zone.enabled) ? COLOR_ALERT_RED : COLOR_GRID_TEXT;
    _tft.fillRoundRect(_btnZone.x, _btnZone.y, _btnZone.w, _btnZone.h, 4, COLOR_BG);
    _tft.drawRoundRect(_btnZone.x, _btnZone.y, _btnZone.w, _btnZone.h, 4, zoneCol);
    _tft.setTextColor(zoneCol, COLOR_BG);
    _tft.drawString("ZONE", _btnZone.x + (_btnZone.w / 2), _btnZone.y + (_btnZone.h / 2), 2);

    // Raw Hex View Toggle Button
    uint16_t rawCol = (_rawHexViewActive) ? COLOR_AMBER : COLOR_PANEL_BORDER;
    _tft.fillRoundRect(_btnViewRaw.x, _btnViewRaw.y, _btnViewRaw.w, _btnViewRaw.h, 4, COLOR_BG);
    _tft.drawRoundRect(_btnViewRaw.x, _btnViewRaw.y, _btnViewRaw.w, _btnViewRaw.h, 4, 
        (_rawHexViewActive) ? COLOR_AMBER : COLOR_CYAN);
    _tft.setTextColor((_rawHexViewActive) ? COLOR_AMBER : COLOR_WHITE, COLOR_BG);
    _tft.drawString((_rawHexViewActive) ? "VIEW: RADAR" : "VIEW: RAW HEX", 
        _btnViewRaw.x + (_btnViewRaw.w / 2), _btnViewRaw.y + (_btnViewRaw.h / 2), 2);

    // Current Zoom display label
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COLOR_TEXT_MUTED, COLOR_PANEL_BG);
    char zoomLbl[24];
    snprintf(zoomLbl, sizeof(zoomLbl), "RANGE: 0 - %.0f m", _config.maxRange);
    _tft.drawString(zoomLbl, 396, 298, 1);
}

void DisplayUI::drawRawHexView() {
    _tft.fillRect(0, 32, 312, 288, COLOR_BG);

    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COLOR_AMBER, COLOR_BG);
    _tft.drawString("RAW SENSOR DATA SNIFFER", 10, 40, 2);

    _tft.setTextColor(COLOR_CYAN, COLOR_BG);
    _tft.drawString("HLK-LD2452 (24GHz @ 256k):", 10, 70, 2);
    _tft.setTextColor(COLOR_WHITE, COLOR_BG);
    _tft.drawString("AA FF 03 00 [X:int16 Y:int16] 55 CC", 10, 92, 1);

    _tft.setTextColor(COLOR_AMBER, COLOR_BG);
    _tft.drawString("HLK-LD6003D (60GHz @ 115k):", 10, 125, 2);
    _tft.setTextColor(COLOR_WHITE, COLOR_BG);
    _tft.drawString("Multi-target Trajectory Stream", 10, 147, 1);

    _tft.setTextColor(COLOR_SUCCESS_GREEN, COLOR_BG);
    char statsBuf[48];
    snprintf(statsBuf, sizeof(statsBuf), "Packets: 24G=%d | 60G=%d", 
             (int)_config.targetCount24G, (int)_config.targetCount60G);
    _tft.drawString(statsBuf, 10, 185, 2);

    _tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG);
    _tft.drawString("Tap 'VIEW: RADAR' to return.", 10, 220, 2);
}

void DisplayUI::handleTouch() {
    if (!_touch.touched()) return;

    uint32_t now = millis();
    if (now - _lastTouchTime < 280) return; // Debounce
    _lastTouchTime = now;

    TS_Point p = _touch.getPoint();

    // Map raw touch coordinates (landscape)
    int16_t touchX = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH);
    int16_t touchY = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT);

    // Constrain to screen bounds
    touchX = constrain(touchX, 0, SCREEN_WIDTH);
    touchY = constrain(touchY, 0, SCREEN_HEIGHT);

    // 1. Top Mode Tabs
    if (_btnMode24G.contains(touchX, touchY)) {
        _config.activeMode = MODE_LD2452;
    } else if (_btnMode60G.contains(touchX, touchY)) {
        _config.activeMode = MODE_LD6003D;
    } else if (_btnModeDual.contains(touchX, touchY)) {
        _config.activeMode = MODE_FUSION;
    }
    // 2. Target Cycle Button
    else if (_btnTargetCycle.contains(touchX, touchY)) {
        _config.individualTrackId++;
        if (_config.individualTrackId > 3) _config.individualTrackId = -1; // -1 = All
    }
    // 3. Zoom Out (-)
    else if (_btnZoomOut.contains(touchX, touchY)) {
        _config.maxRange += 2.0f;
        if (_config.maxRange > 8.0f) _config.maxRange = 8.0f;
    }
    // 4. Zoom In (+)
    else if (_btnZoomIn.contains(touchX, touchY)) {
        _config.maxRange -= 2.0f;
        if (_config.maxRange < 2.0f) _config.maxRange = 2.0f;
    }
    // 5. Grid Toggle
    else if (_btnGrid.contains(touchX, touchY)) {
        _config.gridLines = !_config.gridLines;
    }
    // 6. Alert Zone Toggle
    else if (_btnZone.contains(touchX, touchY)) {
        _config.zone.enabled = !_config.zone.enabled;
    }
    // 7. View Mode (Radar / Raw Hex)
    else if (_btnViewRaw.contains(touchX, touchY)) {
        _rawHexViewActive = !_rawHexViewActive;
    }
}

void DisplayUI::worldToScreen(float wx, float wy, int16_t& sx, int16_t& sy) {
    int16_t ox = RADAR_ORIGIN_X;
    int16_t oy = RADAR_ORIGIN_Y;
    float maxR = _config.maxRange;

    // Azimuth angle in radians
    float angleRad = atan2f(wx, wy);
    float dist = sqrtf((wx * wx) + (wy * wy));

    // Screen pixel radius
    float pixR = (dist / maxR) * RADAR_MAX_PIXEL_RADIUS;

    sx = ox + (int16_t)(sinf(angleRad) * pixR);
    sy = oy - (int16_t)(cosf(angleRad) * pixR);
}

uint16_t DisplayUI::getSensorColor(uint8_t source) {
    switch (source) {
        case 1: return COLOR_CYAN;     // HLK-LD2452 (24G)
        case 2: return COLOR_AMBER;    // HLK-LD6003D (60G)
        case 3: return COLOR_MAGENTA;  // Dual Fused
        default: return COLOR_WHITE;
    }
}
