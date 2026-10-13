#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "Config.h"
#include "SensorFusion.h"

// Color Palette (RGB565 16-bit format)
#define COLOR_BG            0x0842   // Deep Obsidian / Dark Void (#0B0F19)
#define COLOR_PANEL_BG      0x10A5   // Dark Glass Slate (#161D2F)
#define COLOR_PANEL_BORDER  0x296B   // Subtle Blue Border (#2A374D)
#define COLOR_CYAN          0x07FF   // Cyber Cyan / LD2452 (#00E5FF)
#define COLOR_MAGENTA       0xF81F   // Electric Magenta / Fusion Glow (#FF007F)
#define COLOR_AMBER         0xFDA0   // Solar Amber / LD6003D (#FFB300)
#define COLOR_ALERT_RED     0xF800   // Warning Red (#FF1744)
#define COLOR_SUCCESS_GREEN 0x07E0   // Emerald Green (#00E676)
#define COLOR_GRID          0x1927   // Deep Blue Grid (#1A233A)
#define COLOR_GRID_TEXT     0x52AA   // Muted Slate Text (#506080)
#define COLOR_WHITE         0xFFFF
#define COLOR_TEXT_MUTED    0x9CD3

class DisplayUI {
public:
    DisplayUI(SensorFusionEngine& fusion, SystemConfig& config);
    void begin();
    void showSplashScreen(const char* wifiStatus);
    void update();

private:
    TFT_eSPI _tft;
    XPT2046_Touchscreen _touch;
    SensorFusionEngine& _fusion;
    SystemConfig& _config;

    // Animation & timing
    float _sweepAngle;
    uint32_t _lastRenderTime;
    uint32_t _lastTouchTime;
    bool _rawHexViewActive;
    uint32_t _alertFlashTimer;
    bool _flashState;

    // Touch button bounding boxes
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return (px >= x && px <= (x + w) && py >= y && py <= (y + h));
        }
    };

    Rect _btnMode24G;
    Rect _btnMode60G;
    Rect _btnModeDual;
    Rect _btnZoomIn;
    Rect _btnZoomOut;
    Rect _btnGrid;
    Rect _btnZone;
    Rect _btnTargetCycle;
    Rect _btnViewRaw;

    void drawTopBar();
    void drawRadarGrid();
    void drawSweepLine();
    void drawGeofenceZone();
    void drawTargets();
    void drawRightPanel();
    void drawTelemetryWidget();
    void drawControlButtons();
    void drawRawHexView();
    void handleTouch();

    // Coordinate projection helpers
    void worldToScreen(float wx, float wy, int16_t& sx, int16_t& sy);
    uint16_t getSensorColor(uint8_t source);
};
