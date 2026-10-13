// =========================================================================
// RADAR FUSION OS // 24GHz (LD2452) + 60GHz (LD6003D) Dual Tracker
// Exact User Protocol Reference Implementation:
// Output per frame:
//   R,present,x,y,v,present,x,y,v,present,x,y,v    (24G in mm, mm, cm/s)
//   R60,present,x,y,v,present,x,y,v,present,x,y,v  (60G in mm, mm, cm/s)
//   S,bytes24,frames24,bytes60,frames60
//   HEX: <96 hex bytes>                            (24G raw hex)
//   HEX60: <96 hex bytes>                          (60G raw hex)
// NO PLAIN TEXT STRINGS (Zero plain-text logging on USB Serial)
// =========================================================================

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// -------------------------------------------------------------------------
//  TFT DISPLAY CONFIGURATION (Landscape 480x320)
// -------------------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();

#define COLOR_BG        0x0842  // Dark navy
#define COLOR_PANEL     0x1084  // Dark panel background
#define COLOR_GRID      0x18E3  // Faint cyan grid
#define COLOR_GRID_HI   0x2965  // Highlighted grid rings
#define COLOR_CYAN      0x07FF  // 24GHz primary neon cyan
#define COLOR_AMBER     0xFD60  // 60GHz primary amber gold
#define COLOR_MAGENTA   0xF81F  // Dual Fusion electric magenta
#define COLOR_GREEN     0x07E0  // Online green
#define COLOR_ALERT     0xF800  // Alarm red
#define COLOR_TEXT_DIM  0x7BEF  // Dim gray/cyan text
#define COLOR_WHITE     0xFFFF

#define RADAR_CX        145     // Center X of radar sector
#define RADAR_CY        305     // Center Y (bottom anchor of sector)
#define RADAR_RADIUS    260     // Outer radius in pixels (~6.0m max)
#define RADAR_FOV_DEG   60      // Field of view (+/- 60 deg)
#define PANEL_LEFT      295     // Left boundary of right panel
#define PANEL_WIDTH     185

// -------------------------------------------------------------------------
//  RADAR HARDWARE SERIAL PORTS
// -------------------------------------------------------------------------
// 24GHz LD2452 on UART2 (GPIO16=RX, GPIO17=TX) @ 256000 bps
HardwareSerial radar24(2);

// 60GHz LD6003D on UART1 (Default GPIO25=RX, GPIO26=TX) @ 115200 bps
HardwareSerial radar60(1);
int pin60_rx = 25;
int pin60_tx = 26;
uint32_t baud60 = 115200;

// -------------------------------------------------------------------------
//  DATA STRUCTURES
// -------------------------------------------------------------------------
struct RadarTarget {
  bool    valid;
  int16_t x_mm;
  int16_t y_mm;
  int16_t v_cms;
  float   dist_m;
  float   ang_deg;
};

RadarTarget targets24[3];
RadarTarget targets60[3];
RadarTarget targetsFused[3];

int16_t prevBlip_X[3] = {-1, -1, -1};
int16_t prevBlip_Y[3] = {-1, -1, -1};

// -------------------------------------------------------------------------
//  24GHz STATE & EXACT REFERENCE PARSER (LD2452)
// -------------------------------------------------------------------------
uint8_t  f24[30];
uint8_t  n24 = 0;
uint32_t bytes24 = 0, frames24 = 0;
uint8_t  raw24[96];
int      cap24 = 0;

static inline int16_t dec24(uint8_t lo, uint8_t hi) {
  uint16_t v = lo | (hi << 8);
  return (v & 0x8000) ? (int16_t)(v & 0x7FFF) : -(int16_t)v;
}

// Auto baud scan for 24G from user reference
static unsigned long lastB24 = 0;
static int ri24 = 0;
static const uint32_t rates24[] = {256000, 115200, 9600, 19200, 38400, 57600, 460800, 921600};

void update24G() {
  if (frames24 == 0 && millis() - lastB24 > 3000) {
    lastB24 = millis();
    radar24.updateBaudRate(rates24[ri24]);
    Serial.printf("BAUD,%lu\n", (unsigned long)rates24[ri24]);
    ri24 = (ri24 + 1) % 8;
    n24 = 0;
  }

  while (radar24.available()) {
    uint8_t b = radar24.read();
    bytes24++;
    if (cap24 < 96) raw24[cap24++] = b;

    if (n24 == 0 && b != 0xAA) continue;
    f24[n24++] = b;

    if (n24 == 4 && !(f24[1] == 0xFF && f24[2] == 3 && f24[3] == 0)) {
      n24 = 0;
      continue;
    }

    if (n24 == 30) {
      if (f24[28] == 0x55 && f24[29] == 0xCC) {
        frames24++;

        // Output EXACT format from user's working code
        Serial.print("R");
        for (int t = 0; t < 3; t++) {
          const uint8_t *p = &f24[4 + t * 8];
          bool e = true;
          for (int k = 0; k < 8; k++) if (p[k]) e = false;

          if (e) {
            targets24[t].valid = false;
            Serial.print(",0,0,0,0");
          } else {
            int16_t x = dec24(p[0], p[1]);
            int16_t y = dec24(p[2], p[3]);
            int16_t v = dec24(p[4], p[5]);

            targets24[t].valid = true;
            targets24[t].x_mm = x;
            targets24[t].y_mm = y;
            targets24[t].v_cms = v;
            targets24[t].dist_m = sqrtf((float)x * x + (float)y * y) / 1000.0f;
            targets24[t].ang_deg = atan2f((float)x, (float)y) * 180.0f / PI;

            Serial.printf(",1,%d,%d,%d", x, y, v);
          }
        }
        Serial.println();
      }
      n24 = 0;
    }
  }
}

// -------------------------------------------------------------------------
//  60GHz STATE & PARSER (LD6003D)
// -------------------------------------------------------------------------
uint8_t  f60[256];
int      n60 = 0;
uint32_t bytes60 = 0, frames60 = 0;
uint8_t  raw60[96];
int      cap60 = 0;

static inline int16_t dec60HiLink(uint8_t lo, uint8_t hi) {
  uint16_t v = lo | ((uint16_t)hi << 8);
  return (v & 0x8000) ? (int16_t)(v & 0x7FFF) : -(int16_t)v;
}

void parseFrame60_HiLink(const uint8_t* frame, int len) {
  frames60++;
  for (int i = 0; i < 3; i++) targets60[i].valid = false;

  Serial.print("R60");
  int count = (len >= 30) ? 3 : (len - 6) / 8;
  if (count > 3) count = 3;

  for (int t = 0; t < 3; t++) {
    if (t < count) {
      int offset = 4 + t * 8;
      if (offset + 7 < len) {
        const uint8_t *p = &frame[offset];
        bool empty = true;
        for (int k = 0; k < 8; k++) if (p[k]) { empty = false; break; }
        if (!empty) {
          int16_t x = dec60HiLink(p[0], p[1]);
          int16_t y = dec60HiLink(p[2], p[3]);
          int16_t v = dec60HiLink(p[4], p[5]);

          if (y > 50 && y < 9000) {
            targets60[t].valid = true;
            targets60[t].x_mm = x;
            targets60[t].y_mm = y;
            targets60[t].v_cms = v;
            targets60[t].dist_m = sqrtf((float)x * x + (float)y * y) / 1000.0f;
            targets60[t].ang_deg = atan2f((float)x, (float)y) * 180.0f / PI;

            Serial.printf(",1,%d,%d,%d", x, y, v);
            continue;
          }
        }
      }
    }
    Serial.print(",0,0,0,0");
  }
  Serial.println();
}

void parseFrame60_SY(const uint8_t* frame, int len) {
  if (len < 8) return;
  frames60++;
  for (int i = 0; i < 3; i++) targets60[i].valid = false;

  int count = frame[4];
  if (count > 3) count = 3;

  Serial.print("R60");
  int offset = 5;
  for (int t = 0; t < 3; t++) {
    if (t < count && offset + 6 < len) {
      int16_t x_cm = (int16_t)(frame[offset + 1] | (frame[offset + 2] << 8));
      int16_t y_cm = (int16_t)(frame[offset + 3] | (frame[offset + 4] << 8));
      int16_t v_cms = (int16_t)(frame[offset + 5] | (frame[offset + 6] << 8));

      if (y_cm > 5 && y_cm < 850) {
        int16_t x = x_cm * 10;
        int16_t y = y_cm * 10;
        targets60[t].valid = true;
        targets60[t].x_mm = x;
        targets60[t].y_mm = y;
        targets60[t].v_cms = v_cms;
        targets60[t].dist_m = sqrtf((float)x * x + (float)y * y) / 1000.0f;
        targets60[t].ang_deg = atan2f((float)x, (float)y) * 180.0f / PI;

        Serial.printf(",1,%d,%d,%d", x, y, v_cms);
        offset += 7;
        continue;
      }
      offset += 7;
    }
    Serial.print(",0,0,0,0");
  }
  Serial.println();
}

// -------------------------------------------------------------------------
//  60GHz AUTO-PROBE & WAKE ENGINE
//  Automatically cycles pin mapping (25/26 vs 26/25), bauds (115200 vs 256000),
//  and transmits wake/start commands until incoming bytes are confirmed!
// -------------------------------------------------------------------------
static unsigned long lastProbe60 = 0;
static int probeStep60 = 0;

void autoProbe60G() {
  if (bytes60 > 0) return; // Stay permanently locked once bytes arrive!

  if (millis() - lastProbe60 > 2500) {
    lastProbe60 = millis();

    probeStep60 = (probeStep60 + 1) % 4;
    if (probeStep60 == 0) { pin60_rx = 25; pin60_tx = 26; baud60 = 115200; }
    else if (probeStep60 == 1) { pin60_rx = 26; pin60_tx = 25; baud60 = 115200; }
    else if (probeStep60 == 2) { pin60_rx = 25; pin60_tx = 26; baud60 = 256000; }
    else if (probeStep60 == 3) { pin60_rx = 26; pin60_tx = 25; baud60 = 256000; }

    pinMode(pin60_rx, INPUT_PULLUP);
    radar60.begin(baud60, SERIAL_8N1, pin60_rx, pin60_tx);

    // Send wake / continuous reporting trigger commands
    const uint8_t cmdStartReporting[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};
    const uint8_t cmdWakeSY[]         = {0x53, 0x59, 0x01, 0x00, 0x00, 0x54, 0x43};
    const uint8_t cmdHiLinkEnable[]   = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};

    radar60.write(cmdStartReporting, sizeof(cmdStartReporting));
    radar60.write(cmdWakeSY, sizeof(cmdWakeSY));
    radar60.write(cmdHiLinkEnable, sizeof(cmdHiLinkEnable));
  }
}

void update60G() {
  autoProbe60G();

  while (radar60.available()) {
    uint8_t b = radar60.read();
    bytes60++;
    if (cap60 < 96) raw60[cap60++] = b;

    if (n60 == 0) {
      if (b != 0xAA && b != 0x53 && b != 0xFD) continue;
    }

    if (n60 < 255) {
      f60[n60++] = b;
    } else {
      memmove(f60, f60 + 1, 254);
      f60[254] = b;
    }

    if (n60 >= 14 && f60[0] == 0xAA && f60[1] == 0xFF) {
      if (f60[n60 - 2] == 0x55 && f60[n60 - 1] == 0xCC) {
        parseFrame60_HiLink(f60, n60);
        n60 = 0;
        continue;
      }
    } else if (n60 >= 10 && f60[0] == 0x53 && f60[1] == 0x59) {
      if (f60[n60 - 2] == 0x54 && f60[n60 - 1] == 0x43) {
        parseFrame60_SY(f60, n60);
        n60 = 0;
        continue;
      }
    }

    if (n60 > 120) n60 = 0;
  }
}

// -------------------------------------------------------------------------
//  SENSOR FUSION ENGINE
// -------------------------------------------------------------------------
int activeMode = 2; // 0=24G, 1=60G, 2=Dual

void updateFusion() {
  for (int i = 0; i < 3; i++) targetsFused[i].valid = false;

  if (activeMode == 0) {
    for (int i = 0; i < 3; i++) targetsFused[i] = targets24[i];
  } else if (activeMode == 1) {
    for (int i = 0; i < 3; i++) targetsFused[i] = targets60[i];
  } else {
    bool used60[3] = {false, false, false};
    int fusedCount = 0;

    for (int i = 0; i < 3; i++) {
      if (!targets24[i].valid) continue;
      int match60 = -1;
      float bestDist = 650.0f;

      for (int j = 0; j < 3; j++) {
        if (!targets60[j].valid || used60[j]) continue;
        float dx = (float)(targets24[i].x_mm - targets60[j].x_mm);
        float dy = (float)(targets24[i].y_mm - targets60[j].y_mm);
        float d = sqrtf(dx * dx + dy * dy);
        if (d < bestDist) {
          bestDist = d;
          match60 = j;
        }
      }

      if (fusedCount < 3) {
        targetsFused[fusedCount].valid = true;
        if (match60 >= 0) {
          targetsFused[fusedCount].x_mm = (int16_t)(0.4f * targets24[i].x_mm + 0.6f * targets60[match60].x_mm);
          targetsFused[fusedCount].y_mm = (int16_t)(0.4f * targets24[i].y_mm + 0.6f * targets60[match60].y_mm);
          targetsFused[fusedCount].v_cms = (int16_t)(0.7f * targets24[i].v_cms + 0.3f * targets60[match60].v_cms);
          used60[match60] = true;
        } else {
          targetsFused[fusedCount] = targets24[i];
        }
        targetsFused[fusedCount].dist_m = sqrtf((float)targetsFused[fusedCount].x_mm * targetsFused[fusedCount].x_mm +
                                               (float)targetsFused[fusedCount].y_mm * targetsFused[fusedCount].y_mm) / 1000.0f;
        targetsFused[fusedCount].ang_deg = atan2f((float)targetsFused[fusedCount].x_mm, (float)targetsFused[fusedCount].y_mm) * 180.0f / PI;
        fusedCount++;
      }
    }

    for (int j = 0; j < 3; j++) {
      if (targets60[j].valid && !used60[j] && fusedCount < 3) {
        targetsFused[fusedCount] = targets60[j];
        fusedCount++;
      }
    }
  }
}

// -------------------------------------------------------------------------
//  TFT DISPLAY RENDERING (ILI9488 480x320 Landscape)
// -------------------------------------------------------------------------
float sweepAngle = -60.0f;
int   sweepDir = 1;
int16_t prevSweepX = -1, prevSweepY = -1;

bool worldToScreen(int16_t x_mm, int16_t y_mm, int16_t &sx, int16_t &sy) {
  if (y_mm <= 0) return false;
  float scale = (float)RADAR_RADIUS / 6000.0f;
  sx = RADAR_CX + (int16_t)(x_mm * scale);
  sy = RADAR_CY - (int16_t)(y_mm * scale);
  return (sx >= 10 && sx <= (PANEL_LEFT - 10) && sy >= 28 && sy <= RADAR_CY);
}

void drawStaticRadarHUD() {
  tft.fillScreen(COLOR_BG);

  tft.fillRect(0, 0, 480, 24, 0x0185);
  tft.drawFastHLine(0, 24, 480, COLOR_CYAN);
  tft.setTextColor(COLOR_CYAN, 0x0185);
  tft.drawString("RADAR FUSION OS // 24G + 60G SPATIAL TRACKER", 10, 5, 2);

  tft.fillRect(PANEL_LEFT, 25, PANEL_WIDTH, 295, COLOR_PANEL);
  tft.drawFastVLine(PANEL_LEFT, 25, 295, COLOR_GRID_HI);

  float maxAngleRad = (float)RADAR_FOV_DEG * DEG_TO_RAD;
  for (int r = 1; r <= 6; r++) {
    int radPix = (int)((float)r * RADAR_RADIUS / 6.0f);
    float startRad = -HALF_PI - maxAngleRad;
    float endRad   = -HALF_PI + maxAngleRad;

    int prevX = -1, prevY = -1;
    for (float a = startRad; a <= endRad; a += 0.08f) {
      int px = RADAR_CX + (int)(cosf(a) * radPix);
      int py = RADAR_CY + (int)(sinf(a) * radPix);
      if (prevX >= 0) {
        tft.drawLine(prevX, prevY, px, py, (r % 2 == 0) ? COLOR_GRID_HI : COLOR_GRID);
      }
      prevX = px;
      prevY = py;
    }

    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawNumber(r, RADAR_CX + 6, RADAR_CY - radPix - 10, 1);
    tft.drawString("m", RADAR_CX + 14, RADAR_CY - radPix - 10, 1);
  }

  int rays[] = {-60, -45, -30, 0, 30, 45, 60};
  for (int i = 0; i < 7; i++) {
    float a = (float)rays[i] * DEG_TO_RAD - HALF_PI;
    int ex = RADAR_CX + (int)(cosf(a) * RADAR_RADIUS);
    int ey = RADAR_CY + (int)(sinf(a) * RADAR_RADIUS);
    tft.drawLine(RADAR_CX, RADAR_CY, ex, ey, (rays[i] == 0) ? COLOR_CYAN : COLOR_GRID);

    int lx = RADAR_CX + (int)(cosf(a) * (RADAR_RADIUS + 12));
    int ly = RADAR_CY + (int)(sinf(a) * (RADAR_RADIUS + 12));
    if (lx >= 5 && lx <= (PANEL_LEFT - 12) && ly >= 25 && ly <= 315) {
      tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
      tft.drawNumber(rays[i], lx - 8, ly - 5, 1);
    }
  }

  tft.fillCircle(RADAR_CX, RADAR_CY, 5, COLOR_CYAN);
  tft.drawCircle(RADAR_CX, RADAR_CY, 8, COLOR_WHITE);

  tft.setTextColor(COLOR_WHITE, COLOR_PANEL);
  tft.drawString("TELEMETRY HUD", PANEL_LEFT + 10, 32, 2);
  tft.drawFastHLine(PANEL_LEFT + 10, 52, PANEL_WIDTH - 20, COLOR_GRID_HI);
}

void updateDynamicTFT() {
  sweepAngle += 2.0f * (float)sweepDir;
  if (sweepAngle >= 60.0f) { sweepAngle = 60.0f; sweepDir = -1; }
  if (sweepAngle <= -60.0f) { sweepAngle = -60.0f; sweepDir = 1; }

  float swRad = sweepAngle * DEG_TO_RAD - HALF_PI;
  int curSweepX = RADAR_CX + (int)(cosf(swRad) * (RADAR_RADIUS - 4));
  int curSweepY = RADAR_CY + (int)(sinf(swRad) * (RADAR_RADIUS - 4));

  if (prevSweepX >= 0) {
    tft.drawLine(RADAR_CX, RADAR_CY, prevSweepX, prevSweepY, COLOR_BG);
  }
  tft.drawLine(RADAR_CX, RADAR_CY, curSweepX, curSweepY, (activeMode == 1) ? COLOR_AMBER : (activeMode == 2) ? COLOR_MAGENTA : COLOR_CYAN);
  prevSweepX = curSweepX;
  prevSweepY = curSweepY;

  for (int i = 0; i < 3; i++) {
    if (prevBlip_X[i] >= 0) {
      tft.fillCircle(prevBlip_X[i], prevBlip_Y[i], 7, COLOR_BG);
      tft.drawCircle(prevBlip_X[i], prevBlip_Y[i], 12, COLOR_BG);
      prevBlip_X[i] = -1;
      prevBlip_Y[i] = -1;
    }
  }

  for (int i = 0; i < 3; i++) {
    if (!targetsFused[i].valid) continue;

    int16_t sx, sy;
    if (worldToScreen(targetsFused[i].x_mm, targetsFused[i].y_mm, sx, sy)) {
      prevBlip_X[i] = sx;
      prevBlip_Y[i] = sy;

      uint16_t col = (activeMode == 1) ? COLOR_AMBER : (activeMode == 0) ? COLOR_CYAN : COLOR_MAGENTA;
      tft.drawCircle(sx, sy, 11, col);
      tft.drawCircle(sx, sy, 5, COLOR_WHITE);
      tft.fillCircle(sx, sy, 3, col);

      tft.setTextColor(COLOR_WHITE, COLOR_BG);
      char tlbl[12];
      snprintf(tlbl, sizeof(tlbl), "T%d:%.1fm", i + 1, targetsFused[i].dist_m);
      tft.drawString(tlbl, sx + 13, sy - 6, 1);
    }
  }

  tft.fillCircle(RADAR_CX, RADAR_CY, 5, COLOR_CYAN);

  tft.fillRect(PANEL_LEFT + 10, 58, PANEL_WIDTH - 20, 22, 0x020C);
  tft.drawRect(PANEL_LEFT + 10, 58, PANEL_WIDTH - 20, 22, (activeMode == 2) ? COLOR_MAGENTA : (activeMode == 1) ? COLOR_AMBER : COLOR_CYAN);
  tft.setTextColor((activeMode == 2) ? COLOR_MAGENTA : (activeMode == 1) ? COLOR_AMBER : COLOR_CYAN, 0x020C);
  const char *mNames[] = {"MODE: 24G ONLY", "MODE: 60G ONLY", "MODE: DUAL FUSED"};
  tft.drawCentreString(mNames[activeMode], PANEL_LEFT + (PANEL_WIDTH / 2), 62, 2);

  for (int i = 0; i < 3; i++) {
    int cardY = 88 + i * 58;
    tft.fillRect(PANEL_LEFT + 10, cardY, PANEL_WIDTH - 20, 52, 0x0924);
    tft.drawRect(PANEL_LEFT + 10, cardY, PANEL_WIDTH - 20, 52, targetsFused[i].valid ? COLOR_CYAN : COLOR_GRID);

    if (targetsFused[i].valid) {
      tft.setTextColor(COLOR_CYAN, 0x0924);
      char cardHdr[20];
      snprintf(cardHdr, sizeof(cardHdr), "TARGET #%d [TRACKED]", i + 1);
      tft.drawString(cardHdr, PANEL_LEFT + 16, cardY + 4, 1);

      tft.setTextColor(COLOR_WHITE, 0x0924);
      char line1[30];
      snprintf(line1, sizeof(line1), "Dist: %.2fm  Az: %+.0f deg", targetsFused[i].dist_m, targetsFused[i].ang_deg);
      tft.drawString(line1, PANEL_LEFT + 16, cardY + 18, 1);

      char line2[30];
      snprintf(line2, sizeof(line2), "X: %+.2fm   Y: %.2fm", (float)targetsFused[i].x_mm / 1000.0f, (float)targetsFused[i].y_mm / 1000.0f);
      tft.drawString(line2, PANEL_LEFT + 16, cardY + 30, 1);

      tft.setTextColor(COLOR_AMBER, 0x0924);
      char line3[25];
      snprintf(line3, sizeof(line3), "Vel: %+.1f cm/s", (float)targetsFused[i].v_cms);
      tft.drawString(line3, PANEL_LEFT + 16, cardY + 41, 1);
    } else {
      tft.setTextColor(COLOR_TEXT_DIM, 0x0924);
      char emptyHdr[20];
      snprintf(emptyHdr, sizeof(emptyHdr), "TARGET #%d [IDLE]", i + 1);
      tft.drawString(emptyHdr, PANEL_LEFT + 16, cardY + 4, 1);
      tft.drawString("-- No Radar Echo --", PANEL_LEFT + 16, cardY + 22, 1);
    }
  }

  int statY = 266;
  tft.fillRect(PANEL_LEFT + 10, statY, PANEL_WIDTH - 20, 48, 0x0185);
  tft.drawRect(PANEL_LEFT + 10, statY, PANEL_WIDTH - 20, 48, COLOR_GRID_HI);

  tft.setTextColor(COLOR_CYAN, 0x0185);
  char s24[30];
  snprintf(s24, sizeof(s24), "24G: %lu frames", frames24);
  tft.drawString(s24, PANEL_LEFT + 16, statY + 4, 1);

  tft.setTextColor(COLOR_AMBER, 0x0185);
  char s60[30];
  snprintf(s60, sizeof(s60), "60G: %lu frames", frames60);
  tft.drawString(s60, PANEL_LEFT + 16, statY + 18, 1);

  tft.setTextColor(COLOR_GREEN, 0x0185);
  tft.drawString("USB: 115200 LIVE", PANEL_LEFT + 16, statY + 32, 1);
}

// -------------------------------------------------------------------------
//  INBOUND SERIAL COMMANDS
// -------------------------------------------------------------------------
void checkSerialCommands() {
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) continue;

    if (cmd.equalsIgnoreCase("MODE:24")) {
      activeMode = 0;
    } else if (cmd.equalsIgnoreCase("MODE:60")) {
      activeMode = 1;
    } else if (cmd.equalsIgnoreCase("MODE:DUAL")) {
      activeMode = 2;
    } else if (cmd.startsWith("CMD:BAUD60,")) {
      uint32_t nb = cmd.substring(11).toInt();
      if (nb > 0) {
        baud60 = nb;
        radar60.updateBaudRate(baud60);
      }
    } else if (cmd.equalsIgnoreCase("CMD:SWAP60")) {
      int tmp = pin60_rx;
      pin60_rx = pin60_tx;
      pin60_tx = tmp;
      pinMode(pin60_rx, INPUT_PULLUP);
      radar60.begin(baud60, SERIAL_8N1, pin60_rx, pin60_tx);
    } else if (cmd.equalsIgnoreCase("CMD:PING60")) {
      const uint8_t queryCmd[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0x02, 0x00, 0x04, 0x03, 0x02, 0x01};
      radar60.write(queryCmd, sizeof(queryCmd));
    }
  }
}

// -------------------------------------------------------------------------
//  SETUP & MAIN LOOP
// -------------------------------------------------------------------------
unsigned long lastS = 0;
unsigned long lastTftTime = 0;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);

  // Exact reference initialization - NO PLAIN TEXT STRINGS
  radar24.begin(256000, SERIAL_8N1, 16, 17);

  pinMode(pin60_rx, INPUT_PULLUP);
  radar60.begin(baud60, SERIAL_8N1, pin60_rx, pin60_tx);

  tft.init();
  tft.setRotation(1);
  drawStaticRadarHUD();
}

void loop() {
  // Update sensors
  update24G();
  update60G();

  // Handle incoming commands
  checkSerialCommands();

  // Status & Raw Hex every 1000ms (EXACT USER REFERENCE BEHAVIOR)
  if (millis() - lastS > 1000) {
    lastS = millis();
    Serial.printf("S,%lu,%lu,%lu,%lu\n", bytes24, frames24, bytes60, frames60);

    // 24G Raw Hex Dump
    if (cap24 == 96) {
      Serial.print("HEX: ");
      for (int i = 0; i < 96; i++) Serial.printf("%02X ", raw24[i]);
      Serial.println();
      cap24 = 0;
    }

    // 60G Raw Hex Dump
    if (cap60 == 96) {
      Serial.print("HEX60: ");
      for (int i = 0; i < 96; i++) Serial.printf("%02X ", raw60[i]);
      Serial.println();
      cap60 = 0;
    }
  }

  // Update TFT HUD every ~45ms
  unsigned long now = millis();
  if (now - lastTftTime >= 45) {
    lastTftTime = now;
    updateFusion();
    updateDynamicTFT();
  }
}
