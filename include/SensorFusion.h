#pragma once
#include <Arduino.h>
#include "Config.h"
#include "LD2452.h"
#include "LD6003D.h"

class SensorFusionEngine {
public:
    SensorFusionEngine(LD2452Driver& ld2452, LD6003DDriver& ld6003d, SystemConfig& config);
    void begin();
    void update();

    // Access fused output
    uint8_t getTargetCount() const { return _displayTargetCount; }
    const Target& getTarget(uint8_t index) const { return _displayTargets[index]; }

    // Geofence status
    bool isAlertTriggered() const { return _config.alertActive; }

private:
    LD2452Driver& _radar24G;
    LD6003DDriver& _radar60G;
    SystemConfig& _config;

    Target _displayTargets[MAX_FUSED_TARGETS];
    uint8_t _displayTargetCount;

    void processSingleMode(uint8_t sensorId);
    void processFusionMode();
    void checkGeofenceAlerts();
    void updateTrail(Target& target, float x, float y);
};
