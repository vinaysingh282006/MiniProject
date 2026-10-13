#include <Arduino.h>
#include "Config.h"
#include "LD2452.h"
#include "LD6003D.h"
#include "SensorFusion.h"
#include "DisplayUI.h"
#include "WebServerManager.h"

// =========================================================================
//  GLOBAL INSTANCES
// =========================================================================
SystemConfig config = {
    .activeMode = MODE_FUSION,        // Default: Dual Sensor Fusion Mode
    .maxRange = 6.0f,                 // Default: 6 meters range
    .gridLines = true,                // Enable radar grid lines
    .showTrails = true,               // Enable breadcrumb trajectory trails
    .flashAlert = true,               // Flash UI on geofence intrusion
    .buzzerEnabled = true,            // Sound alert buzzer
    .individualTrackId = -1,          // -1 = Track all individuals, 1..3 = focus
    .zone = {
        .enabled = true,
        .xMin = -1.5f,
        .xMax =  1.5f,
        .yMin =  1.0f,
        .yMax =  3.5f
    },
    .alertActive = false,
    .alertTimestamp = 0,
    .targetCount24G = 0,
    .targetCount60G = 0,
    .targetCountFused = 0,
    .fps = 0.0f
};

// Drivers & Engines
LD2452Driver       radar24;
LD6003DDriver      radar60;
SensorFusionEngine fusion(radar24, radar60, config);
DisplayUI          display(fusion, config);
WebServerManager   webServer(fusion, config, radar24, radar60);

// Timing variables
uint32_t lastSerialLog = 0;

void printBanner() {
    Serial.println("\n\n============================================================");
    Serial.println("  RADAR FUSION OS // 24GHz + 60GHz mmWave Spatial Tracker");
    Serial.println("  Hardware: ESP32 DevKit V1 | ILI9488 3.5\" SPI TFT");
    Serial.println("  Sensors: HLK-LD2452 (24G) & HLK-LD6003D (60G)");
    Serial.println("============================================================");
}

void setup() {
    // 1. USB Serial Monitor
    Serial.begin(115200);
    delay(500);
    printBanner();

    // 2. Alert Buzzer Pin setup
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    // 3. Initialize Display & Touch
    Serial.println("[INIT] Initializing 3.5\" ILI9488 SPI TFT Display...");
    display.begin();

    // 4. Initialize Radar 1: HLK-LD2452 (24 GHz) on UART1
    Serial.printf("[INIT] Starting HLK-LD2452 (24G) on RX=%d, TX=%d @ %d baud...\n",
                  PIN_LD2452_RX, PIN_LD2452_TX, LD2452_BAUD_RATE);
    radar24.begin(Serial1, PIN_LD2452_RX, PIN_LD2452_TX, LD2452_BAUD_RATE);

    // 5. Initialize Radar 2: HLK-LD6003D (60 GHz) on UART2
    Serial.printf("[INIT] Starting HLK-LD6003D (60G) on RX=%d, TX=%d @ %d baud...\n",
                  PIN_LD6003D_RX, PIN_LD6003D_TX, LD6003D_BAUD_RATE);
    radar60.begin(Serial2, PIN_LD6003D_RX, PIN_LD6003D_TX, LD6003D_BAUD_RATE);

    // 6. Initialize Sensor Fusion Engine
    fusion.begin();

    // 7. Initialize Web Server & WiFi
    Serial.println("[INIT] Connecting WiFi & starting Web Server...");
    webServer.begin();

    String ipMsg = String("WiFi IP: ") + webServer.getIPAddress();
    if (!webServer.isConnectedSTA()) {
        ipMsg = String("SoftAP IP: ") + webServer.getIPAddress() + " (RadarFusion-AP)";
    }
    Serial.println(String("[WIFI] Web Dashboard online at http://") + webServer.getIPAddress());

    // 8. Show Modern Boot Splash Screen with animation & hardware checks
    display.showSplashScreen(ipMsg.c_str());

    Serial.println("[READY] Radar Fusion Engine Running!");
}

void loop() {
    // 1. Update Sensor Acquisition & Fusion Engine
    fusion.update();

    // 2. Update TFT Display & Touch Input
    display.update();

    // 3. Update Web Server & WebSocket Streaming
    webServer.update();

    // 4. Buzzer alert trigger on geofence perimeter breach
    if (config.alertActive && config.buzzerEnabled) {
        // Intermittent alarm beeping
        if ((millis() / 200) % 2 == 0) {
            digitalWrite(PIN_BUZZER, HIGH);
        } else {
            digitalWrite(PIN_BUZZER, LOW);
        }
    } else {
        digitalWrite(PIN_BUZZER, LOW);
    }

    // 5. Periodic Serial Diagnostic Log (every 1 second)
    if (millis() - lastSerialLog >= 1000) {
        lastSerialLog = millis();

        uint8_t count = fusion.getTargetCount();
        const char* modeName = (config.activeMode == MODE_FUSION) ? "DUAL FUSION" :
                               (config.activeMode == MODE_LD6003D) ? "60G ONLY" : "24G ONLY";

        Serial.printf("[STATUS] Mode: %s | Active Targets: %d | Alert: %s | FPS: %.1f\n",
                      modeName, count,
                      config.alertActive ? "BREACH!" : "NORMAL",
                      config.fps);

        for (uint8_t i = 0; i < count; i++) {
            const Target& t = fusion.getTarget(i);
            const char* srcStr = (t.sensorSource == 3) ? "DUAL" : (t.sensorSource == 2) ? "60G" : "24G";
            Serial.printf("  -> Target #%d [%s]: Dist=%.2fm, Ang=%+.1f deg, Spd=%+.2fm/s, (X=%+.2f, Y=%.2f) %s\n",
                          t.id, srcStr, t.distance, t.angle, t.speed, t.x, t.y,
                          t.inAlertZone ? "[IN ALERT ZONE]" : "");
        }
    }
}
