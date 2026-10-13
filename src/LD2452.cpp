#include "LD2452.h"
#include <cmath>

LD2452Driver::LD2452Driver() 
    : _serial(nullptr), _activeCount(0), _connected(false), 
      _lastPacketTime(0), _packetCount(0), _rxIndex(0), _latestRawHex("") {
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
}

void LD2452Driver::update() {
    if (!_serial) return;

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

        // Raw 16-bit values (Little Endian)
        uint16_t rawX = frame[offset]     | ((uint16_t)frame[offset + 1] << 8);
        uint16_t rawY = frame[offset + 2] | ((uint16_t)frame[offset + 3] << 8);
        uint16_t rawS = frame[offset + 4] | ((uint16_t)frame[offset + 5] << 8);
        uint16_t rawR = frame[offset + 6] | ((uint16_t)frame[offset + 7] << 8);

        // Hi-Link Sign conversion (bit 15 is sign bit, 0x8000 indicates negative)
        int16_t x_mm = (rawX & 0x8000) ? -(int16_t)(rawX & 0x7FFF) : (int16_t)(rawX & 0x7FFF);
        int16_t y_mm = (rawY & 0x8000) ? -(int16_t)(rawY & 0x7FFF) : (int16_t)(rawY & 0x7FFF);
        int16_t s_cm = (rawS & 0x8000) ? -(int16_t)(rawS & 0x7FFF) : (int16_t)(rawS & 0x7FFF);

        // Convert to meters and m/s
        float x_m = (float)x_mm / 1000.0f;
        float y_m = (float)y_mm / 1000.0f;
        float speed_ms = (float)s_cm / 100.0f;

        // Euclidean distance and azimuth angle
        float dist_m = sqrtf((x_m * x_m) + (y_m * y_m));
        float angle_deg = atan2f(x_m, y_m) * (180.0f / M_PI);

        Target& tgt = _targets[i];
        tgt.id = i + 1;

        // Target is valid if distance is greater than 0.1m and within 8m range
        if (dist_m > 0.10f && dist_m < 8.5f && y_m > 0.05f) {
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
