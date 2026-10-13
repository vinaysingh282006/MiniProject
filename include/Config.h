#pragma once
#include <Arduino.h>

// =========================================================================
//  HARDWARE WIRING & PIN DEFINITIONS
// =========================================================================

// --- Radar 1: HLK-LD2452 (24 GHz FMCW Tracking Radar) ---
// Connected to ESP32 Hardware Serial 1
#define PIN_LD2452_RX         16      // ESP32 RX1 <- HLK-LD2452 TX
#define PIN_LD2452_TX         17      // ESP32 TX1 -> HLK-LD2452 RX
#define LD2452_BAUD_RATE      256000  // Default baud rate for HLK-LD2452

// --- Radar 2: HLK-LD6003D (60 GHz FMCW Trajectory Radar) ---
// Connected to ESP32 Hardware Serial 2
// NOTE: Use TX0 & RX0 on the LD6003 module (TX1/RX1 is debug, TX2/RX2 is flash/ISP)
#define PIN_LD6003D_RX        25      // ESP32 RX2 <- HLK-LD6003D TX0 (Module Transmit)
#define PIN_LD6003D_TX        26      // ESP32 TX2 -> HLK-LD6003D RX0 (Module Receive)
#define LD6003D_BAUD_RATE     115200  // Default baud rate for HLK-LD6003D

// --- Display: 3.5" SPI TFT (ILI9488 480x320) ---
// Uses VSPI bus on ESP32
#define PIN_TFT_MISO          19      // TFT SDO / MISO
#define PIN_TFT_MOSI          23      // TFT SDI / MOSI
#define PIN_TFT_SCLK          18      // TFT SCK / SCLK
#define PIN_TFT_CS            15      // TFT Chip Select
#define PIN_TFT_DC            2       // TFT Data / Command
#define PIN_TFT_RST           4       // TFT Reset
#define PIN_TFT_BL            27      // TFT Backlight Control (PWM / High)

// --- Touch: XPT2046 Resistive Touch Controller ---
// Shares VSPI bus with TFT
#define PIN_TOUCH_CS          21      // Touch Chip Select (T_CS)
#define PIN_TOUCH_IRQ         22      // Touch Pen Interrupt (T_IRQ)

// --- Optional Alert Peripherals ---
#define PIN_BUZZER            32      // Optional active buzzer for geofence breach

// =========================================================================
//  DISPLAY DIMENSIONS & TOUCH CALIBRATION CONSTANTS
// =========================================================================
#define SCREEN_WIDTH          480
#define SCREEN_HEIGHT         320

// Touch Screen Calibration Min/Max (Orientation: Landscape 1 or 3)
#define TOUCH_MIN_X           300
#define TOUCH_MAX_X           3800
#define TOUCH_MIN_Y           300
#define TOUCH_MAX_Y           3800

// =========================================================================
//  RADAR & SENSOR MODES
// =========================================================================
enum SensorMode : uint8_t {
    MODE_LD2452   = 0,   // 24 GHz Sensor Only
    MODE_LD6003D  = 1,   // 60 GHz Sensor Only
    MODE_FUSION   = 2    // Dual Sensor Fusion (Both Active Simultaneously)
};

// =========================================================================
//  TARGET DATA STRUCTURES
// =========================================================================
#define MAX_TARGETS_PER_SENSOR 4
#define MAX_FUSED_TARGETS      6
#define TARGET_TRAIL_LENGTH    8

struct Point2D {
    float x; // in meters (lateral: -left, +right)
    float y; // in meters (longitudinal: forward)
};

struct Target {
    uint8_t  id;             // Target Track ID (1..4)
    bool     valid;          // Active detection flag
    float    x;              // Lateral coordinate (meters)
    float    y;              // Distance coordinate (meters)
    float    distance;       // Euclidean distance R = sqrt(X^2 + Y^2) (meters)
    float    angle;          // Azimuth angle in degrees (-60 to +60)
    float    speed;          // Radial velocity (cm/s or m/s)
    uint16_t resolution;     // Distance resolution gate (mm)
    uint8_t  sensorSource;   // 1 = LD2452 (24G), 2 = LD6003D (60G), 3 = Fused Dual
    uint8_t  confidence;     // Tracking confidence (0..100%)
    uint32_t lastSeenMs;     // Timestamp of last update
    bool     inAlertZone;    // True if target currently inside geofenced alert zone
    
    // Breadcrumb trail history for modern radar trajectory rendering
    Point2D  trail[TARGET_TRAIL_LENGTH];
    uint8_t  trailCount;
};

// =========================================================================
//  GEOFENCE / ALERT ZONE CONFIGURATION
// =========================================================================
struct AlertZone {
    bool  enabled;
    float xMin;   // meters (e.g. -1.5m)
    float xMax;   // meters (e.g. +1.5m)
    float yMin;   // meters (e.g.  1.0m)
    float yMax;   // meters (e.g.  3.5m)
};

// =========================================================================
//  GLOBAL SYSTEM CONFIGURATION
// =========================================================================
struct SystemConfig {
    SensorMode  activeMode;          // Current active sensor mode
    float       maxRange;            // Current display zoom range (2.0m, 4.0m, 6.0m, 8.0m)
    bool        gridLines;           // Show polar/cartesian grid
    bool        showTrails;          // Show movement breadcrumb trails
    bool        flashAlert;          // Flash UI when geofence is breached
    bool        buzzerEnabled;       // Sound buzzer on intrusion
    int8_t      individualTrackId;   // -1 = Track All, 1..4 = Focus on individual person
    AlertZone   zone;                // Custom geofence perimeter
    bool        alertActive;         // Current alert trigger flag
    uint32_t    alertTimestamp;      // Time of alert trigger
    uint32_t    targetCount24G;      // Active targets count from 24GHz
    uint32_t    targetCount60G;      // Active targets count from 60GHz
    uint32_t    targetCountFused;    // Active targets count in fusion mode
    float       fps;                 // UI Render Frame Rate
};

// =========================================================================
//  WIFI & NETWORK CONFIGURATION
// =========================================================================
// Change these to your home/college Wi-Fi credentials
#define DEFAULT_WIFI_SSID     "YourWiFiSSID"
#define DEFAULT_WIFI_PASS     "YourWiFiPassword"

// SoftAP credentials (if Wi-Fi fails to connect, creates its own hotspot)
#define AP_SSID               "RadarFusion-AP"
#define AP_PASS               "12345678"
#define WEB_SERVER_PORT       80
#define WS_PATH               "/ws"
