#include "LD2452.h"
#include <cmath>

// Hi-Link coordinate decoder matching user verified formula:
// When bit 15 is 1, value is positive (v & 0x7FFF); otherwise value is negative (-v).
static inline int16_t decodeHiLinkValue(uint8_t lo, uint8_t hi) {
    uint16_t v = lo | ((uint16_t)hi << 8);
    return (v & 0x8000) ? (int16_t)(v & 0x7FFF) : -(int16_t)v;
}

LD2452Driver::LD2452Driver() 
    : _serial(nullptr), _activeCount(0), _connected(false), 
      _lastPacketTime(0), _packetCount(0), _bytesReceived(0),
      _lastStatsTime(0), _rxIndex(0), _latestRawHex("") {
    for (int i = 0; i < MAX_TARGETS_PER_SENSOR; i++) {
        _targets[i].id = i + 1;
        _targets[i].valid = false;
        _targets[i].sensorSource = 1; // LD2452
        _targets[i].confidence = 0;
        _targets[i].trailCount = 0;
    }
}

void LD2452Driver::begin(HardwareSerial& serialPort, int rxPin, int txPin, uint32_t baudRate) {
    _serial = &serialPort;
    _serial->begin(baudRate, SERIAL_8N1, rxPin, txPin);
    _lastPacketTime = millis();
    _lastStatsTime = millis();
}

void LD2452Driver::update() {
    if (!_serial) return;

    // Output periodic 1-second telemetry stats matching user script (S,bytes,frames)
    if (millis() - _lastStatsTime >= 1000) {
        _lastStatsTime = millis();
        Serial.printf("S,%lu,%lu\n", _bytesReceived, _packetCount);
    }

    // Timeout check: if no frame received in last 2 seconds, mark disconnected
    if (millis() - _lastPacketTime > 2000) {
        _connected = false;
        for (int i = 0; i < MAX_TARGETS_PER_SENSOR; i++) {
            _targets[i].valid = false;
        }
        _activeCount = 0;
    }

    while (_serial->available() > 0) {
        uint8_t byteIn = _serial->read();
        _bytesReceived++;

        // State machine matching frame header: AA FF 03 00
        if (_rxIndex == 0) {
            if (byteIn == 0xAA) _rxBuffer[_rxIndex++] = byteIn;
        } else if (_rxIndex == 1) {
            if (byteIn == 0xFF) _rxBuffer[_rxIndex++] = byteIn;
            else _rxIndex = 0;
        } else if (_rxIndex == 2) {
            if (byteIn == 0x03) _rxBuffer[_rxIndex++] = byteIn;
            else _rxIndex = 0;
        } else if (_rxIndex == 3) {
            if (byteIn == 0x00) _rxBuffer[_rxIndex++] = byteIn;
            else _rxIndex = 0;
        } else {
            // Collecting remaining payload (bytes 4 to 29)
            _rxBuffer[_rxIndex++] = byteIn;

            if (_rxIndex >= FRAME_LEN) {
                // Check footer: 55 CC
                if (_rxBuffer[28] == 0x55 && _rxBuffer[29] == 0xCC) {
                    parseFrame(_rxBuffer);
                    _connected = true;
                    _lastPacketTime = millis();
                    _packetCount++;
                }
                _rxIndex = 0; // Reset for next frame
            }
        }
    }
}

void LD2452Driver::parseFrame(const uint8_t* frame) {
    // Save raw hex for web and debug monitor
    char hexBuffer[70];
    snprintf(hexBuffer, sizeof(hexBuffer), 
        "%02X %02X %02X %02X | %02X %02X %02X %02X ... %02X %02X",
        frame[0], frame[1], frame[2], frame[3],
        frame[4], frame[5], frame[6], frame[7],
        frame[28], frame[29]);
    _latestRawHex = String(hexBuffer);

    uint8_t validCount = 0;

    // Up to 3 targets in LD2452 frame (slots 0, 1, 2)
    for (int i = 0; i < 3; i++) {
        size_t offset = 4 + (i * 8);
        const uint8_t* p = &frame[offset];

        // Check if all 8 bytes are 0 (empty target slot)
        bool empty = true;
        for (int k = 0; k < 8; k++) {
            if (p[k] != 0) {
                empty = false;
                break;
            }
        }

        Target& tgt = _targets[i];
        tgt.id = i + 1;

        if (empty) {
            tgt.valid = false;
            continue;
        }

        // Decode coordinates using user-verified sign convention
        int16_t x_mm = decodeHiLinkValue(p[0], p[1]);
        int16_t y_mm = decodeHiLinkValue(p[2], p[3]);
        int16_t s_cm = decodeHiLinkValue(p[4], p[5]);
        uint16_t rawR = p[6] | ((uint16_t)p[7] << 8);

        // Convert to meters and m/s
        float x_m = (float)x_mm / 1000.0f;
        float y_m = (float)y_mm / 1000.0f;
        float speed_ms = (float)s_cm / 100.0f;

        // Euclidean distance and azimuth angle
        float dist_m = sqrtf((x_m * x_m) + (y_m * y_m));
        float angle_deg = atan2f(x_m, y_m) * (180.0f / M_PI);

        // Target is valid if distance is within range and in front of radar
        if (dist_m > 0.05f && dist_m < 9.0f && y_m > 0.0f) {
            tgt.valid = true;
            tgt.x = x_m;
            tgt.y = y_m;
            tgt.distance = dist_m;
            tgt.angle = angle_deg;
            tgt.speed = speed_ms;
            tgt.resolution = rawR;
            tgt.sensorSource = 1; // HLK-LD2452 (24 GHz)
            tgt.confidence = 90;
            tgt.lastSeenMs = millis();
            updateTrail(tgt, x_m, y_m);
            validCount++;
        } else {
            tgt.valid = false;
        }
    }

    _activeCount = validCount;

    // Stream 100% compatible radar.html output format over USB Serial:
    // Output: R,present,x,y,v, present,x,y,v, present,x,y,v (in mm, mm, cm/s)
    Serial.print("R");
    for (int t = 0; t < 3; t++) {
        const uint8_t *p = &frame[4 + t * 8];
        bool empty = true;
        for (int k = 0; k < 8; k++) {
            if (p[k] != 0) { empty = false; break; }
        }
        if (empty) {
            Serial.print(",0,0,0,0");
        } else {
            Serial.printf(",1,%d,%d,%d", decodeHiLinkValue(p[0], p[1]),
                                         decodeHiLinkValue(p[2], p[3]),
                                         decodeHiLinkValue(p[4], p[5]));
        }
    }
    Serial.println();
}

void LD2452Driver::updateTrail(Target& target, float x, float y) {
    // Shift trail points back
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
