#include "SensorFusion.h"
#include <cmath>

SensorFusionEngine::SensorFusionEngine(LD2452Driver& ld2452, LD6003DDriver& ld6003d, SystemConfig& config)
    : _radar24G(ld2452), _radar60G(ld6003d), _config(config), _displayTargetCount(0) {
    for (int i = 0; i < MAX_FUSED_TARGETS; i++) {
        _displayTargets[i].id = i + 1;
        _displayTargets[i].valid = false;
        _displayTargets[i].trailCount = 0;
    }
}

void SensorFusionEngine::begin() {
    _displayTargetCount = 0;
}

void SensorFusionEngine::update() {
    // Read raw sensors
    _radar24G.update();
    _radar60G.update();

    _config.targetCount24G = _radar24G.getActiveTargetCount();
    _config.targetCount60G = _radar60G.getActiveTargetCount();

    switch (_config.activeMode) {
        case MODE_LD2452:
            processSingleMode(1);
            break;
        case MODE_LD6003D:
            processSingleMode(2);
            break;
        case MODE_FUSION:
            processFusionMode();
            break;
    }

    // Check geofence perimeter intrusions
    checkGeofenceAlerts();
}

void SensorFusionEngine::processSingleMode(uint8_t sensorId) {
    _displayTargetCount = 0;

    if (sensorId == 1) { // LD2452 (24 GHz)
        for (int i = 0; i < MAX_TARGETS_PER_SENSOR; i++) {
            const Target& t = _radar24G.getTarget(i);
            if (t.valid) {
                // If individual tracking filter is set, only show matching ID
                if (_config.individualTrackId == -1 || _config.individualTrackId == t.id) {
                    _displayTargets[_displayTargetCount] = t;
                    _displayTargetCount++;
                }
            }
        }
    } else { // LD6003D (60 GHz)
        for (int i = 0; i < MAX_TARGETS_PER_SENSOR; i++) {
            const Target& t = _radar60G.getTarget(i);
            if (t.valid) {
                if (_config.individualTrackId == -1 || _config.individualTrackId == t.id) {
                    _displayTargets[_displayTargetCount] = t;
                    _displayTargetCount++;
                }
            }
        }
    }

    _config.targetCountFused = _displayTargetCount;
}

void SensorFusionEngine::processFusionMode() {
    _displayTargetCount = 0;
    bool matched60G[MAX_TARGETS_PER_SENSOR] = {false};

    // Iterate through LD2452 targets and correlate with LD6003D targets
    for (int i = 0; i < MAX_TARGETS_PER_SENSOR; i++) {
        const Target& t24 = _radar24G.getTarget(i);
        if (!t24.valid) continue;

        int bestMatchIndex = -1;
        float minDelta = 0.50f; // 50 cm correlation radius

        for (int j = 0; j < MAX_TARGETS_PER_SENSOR; j++) {
            if (matched60G[j]) continue;
            const Target& t60 = _radar60G.getTarget(j);
            if (!t60.valid) continue;

            float dx = t24.x - t60.x;
            float dy = t24.y - t60.y;
            float distDelta = sqrtf((dx * dx) + (dy * dy));

            if (distDelta < minDelta) {
                minDelta = distDelta;
                bestMatchIndex = j;
            }
        }

        if (bestMatchIndex >= 0 && _displayTargetCount < MAX_FUSED_TARGETS) {
            // MATCH FOUND -> DUAL FUSED TARGET
            matched60G[bestMatchIndex] = true;
            const Target& t60 = _radar60G.getTarget(bestMatchIndex);

            Target fused;
            fused.id = _displayTargetCount + 1;
            fused.valid = true;
            // Weighted average fusion (60GHz has slightly higher angular resolution, 24GHz has strong range)
            fused.x = (t24.x * 0.45f) + (t60.x * 0.55f);
            fused.y = (t24.y * 0.50f) + (t60.y * 0.50f);
            fused.distance = sqrtf((fused.x * fused.x) + (fused.y * fused.y));
            fused.angle = atan2f(fused.x, fused.y) * (180.0f / M_PI);
            fused.speed = (t24.speed + t60.speed) * 0.5f;
            fused.resolution = (t24.resolution + t60.resolution) / 2;
            fused.sensorSource = 3; // Fused Dual
            fused.confidence = 99;  // Highest confidence: verified by both RF spectra!
            fused.lastSeenMs = millis();
            fused.trailCount = t24.trailCount;
            for (int k = 0; k < t24.trailCount; k++) fused.trail[k] = t24.trail[k];
            updateTrail(fused, fused.x, fused.y);

            if (_config.individualTrackId == -1 || _config.individualTrackId == fused.id) {
                _displayTargets[_displayTargetCount++] = fused;
            }
        } else if (_displayTargetCount < MAX_FUSED_TARGETS) {
            // ONLY LD2452 SEES THIS TARGET
            if (_config.individualTrackId == -1 || _config.individualTrackId == t24.id) {
                _displayTargets[_displayTargetCount] = t24;
                _displayTargets[_displayTargetCount].confidence = 75;
                _displayTargetCount++;
            }
        }
    }

    // Add any remaining LD6003D targets that were not matched
    for (int j = 0; j < MAX_TARGETS_PER_SENSOR; j++) {
        if (!matched60G[j] && _radar60G.getTarget(j).valid && _displayTargetCount < MAX_FUSED_TARGETS) {
            const Target& t60 = _radar60G.getTarget(j);
            if (_config.individualTrackId == -1 || _config.individualTrackId == t60.id) {
                _displayTargets[_displayTargetCount] = t60;
                _displayTargets[_displayTargetCount].confidence = 75;
                _displayTargetCount++;
            }
        }
    }

    _config.targetCountFused = _displayTargetCount;
}

void SensorFusionEngine::checkGeofenceAlerts() {
    bool anyAlert = false;

    if (_config.zone.enabled) {
        for (int i = 0; i < _displayTargetCount; i++) {
            Target& t = _displayTargets[i];
            if (t.valid) {
                if (t.x >= _config.zone.xMin && t.x <= _config.zone.xMax &&
                    t.y >= _config.zone.yMin && t.y <= _config.zone.yMax) {
                    t.inAlertZone = true;
                    anyAlert = true;
                } else {
                    t.inAlertZone = false;
                }
            }
        }
    }

    _config.alertActive = anyAlert;
    if (anyAlert) {
        _config.alertTimestamp = millis();
    }
}

void SensorFusionEngine::updateTrail(Target& target, float x, float y) {
    if (target.trailCount < TARGET_TRAIL_LENGTH) {
        target.trail[target.trailCount].x = x;
        target.trail[target.trailCount].y = y;
        target.trailCount++;
    } else {
        for (int i = 0; i < TARGET_TRAIL_LENGTH - 1; i++) {
            target.trail[i] = target.trail[i + 1];
        }
        target.trail[TARGET_TRAIL_LENGTH - 1].x = x;
        target.trail[TARGET_TRAIL_LENGTH - 1].y = y;
    }
}
