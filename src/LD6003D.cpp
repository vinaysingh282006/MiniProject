#include "LD6003D.h"
#include <cmath>

LD6003DDriver::LD6003DDriver()
    : _serial(nullptr), _activeCount(0), _connected(false),
      _lastPacketTime(0), _packetCount(0), _rxIndex(0), _latestRawHex("") {
    for (int i = 0; i < MAX_TARGETS_PER_SENSOR; i++) {
        _targets[i].id = i + 1;
        _targets[i].valid = false;
        _targets[i].sensorSource = 2; // LD6003D (60GHz)
        _targets[i].confidence = 0;
        _targets[i].trailCount = 0;
    }
}

void LD6003DDriver::begin(HardwareSerial& serialPort, int rxPin, int txPin, uint32_t baudRate) {
    _serial = &serialPort;
    _serial->begin(baudRate, SERIAL_8N1, rxPin, txPin);
    _lastPacketTime = millis();
}

void LD6003DDriver::update() {
    if (!_serial) return;

    // Timeout: 2 seconds without packets indicates disconnected
    if (millis() - _lastPacketTime > 2000) {
        _connected = false;
        for (int i = 0; i < MAX_TARGETS_PER_SENSOR; i++) {
            _targets[i].valid = false;
        }
        _activeCount = 0;
    }

    while (_serial->available() > 0) {
        uint8_t byteIn = _serial->read();

        if (_rxIndex < MAX_BUF_LEN) {
            _rxBuffer[_rxIndex++] = byteIn;
        } else {
            // Buffer overflow protection, shift buffer
            memmove(_rxBuffer, _rxBuffer + 1, MAX_BUF_LEN - 1);
            _rxBuffer[MAX_BUF_LEN - 1] = byteIn;
        }

        // Protocol Check 1: Standard Hi-Link Multi-Target Frame (AA FF ... 55 CC)
        if (_rxIndex >= 4 && _rxBuffer[0] == 0xAA && _rxBuffer[1] == 0xFF) {
            // Check if end marker 55 CC has been received
            if (_rxIndex >= 14 && _rxBuffer[_rxIndex - 2] == 0x55 && _rxBuffer[_rxIndex - 1] == 0xCC) {
                parseStandardFrame(_rxBuffer, _rxIndex);
                _connected = true;
                _lastPacketTime = millis();
                _packetCount++;
                _rxIndex = 0;
                continue;
            }
        } 
        // Protocol Check 2: SY / ADT6101 Protocol Frame (53 59 ... 54 43)
        else if (_rxIndex >= 2 && _rxBuffer[0] == 0x53 && _rxBuffer[1] == 0x59) {
            if (_rxIndex >= 10 && _rxBuffer[_rxIndex - 2] == 0x54 && _rxBuffer[_rxIndex - 1] == 0x43) {
                parseSYFrame(_rxBuffer, _rxIndex);
                _connected = true;
                _lastPacketTime = millis();
                _packetCount++;
                _rxIndex = 0;
                continue;
            }
        } 
        // Resync: If first byte doesn't match known headers, discard
        else if (_rxIndex == 1) {
            if (byteIn != 0xAA && byteIn != 0x53 && byteIn != 0xFD) {
                _rxIndex = 0;
            }
        }
    }
}

void LD6003DDriver::parseStandardFrame(const uint8_t* frame, size_t length) {
    char hexBuffer[70];
    snprintf(hexBuffer, sizeof(hexBuffer),
        "%02X %02X %02X %02X | LEN:%d | %02X %02X",
        frame[0], frame[1], frame[2], frame[3], (int)length,
        frame[length - 2], frame[length - 1]);
    _latestRawHex = String(hexBuffer);

    uint8_t validCount = 0;
    // Calculate number of targets based on packet length (each target 8 bytes)
    size_t targetCount = (length >= 30) ? 3 : (length - 6) / 8;
    if (targetCount > MAX_TARGETS_PER_SENSOR) targetCount = MAX_TARGETS_PER_SENSOR;

    for (size_t i = 0; i < targetCount; i++) {
        size_t offset = 4 + (i * 8);
        if (offset + 7 >= length) break;
        const uint8_t* p = &frame[offset];

        // Check if target slot is empty
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

        // Hi-Link Sign conversion (bit 15 is 1 for positive)
        uint16_t rawX = p[0] | ((uint16_t)p[1] << 8);
        uint16_t rawY = p[2] | ((uint16_t)p[3] << 8);
        uint16_t rawS = p[4] | ((uint16_t)p[5] << 8);
        uint16_t rawR = p[6] | ((uint16_t)p[7] << 8);

        int16_t x_mm = (rawX & 0x8000) ? (int16_t)(rawX & 0x7FFF) : -(int16_t)rawX;
        int16_t y_mm = (rawY & 0x8000) ? (int16_t)(rawY & 0x7FFF) : -(int16_t)rawY;
        int16_t s_cm = (rawS & 0x8000) ? (int16_t)(rawS & 0x7FFF) : -(int16_t)rawS;

        float x_m = (float)x_mm / 1000.0f;
        float y_m = (float)y_mm / 1000.0f;
        float speed_ms = (float)s_cm / 100.0f;

        float dist_m = sqrtf((x_m * x_m) + (y_m * y_m));
        float angle_deg = atan2f(x_m, y_m) * (180.0f / M_PI);

        if (dist_m > 0.05f && dist_m < 9.0f && y_m > 0.0f) {
            tgt.valid = true;
            tgt.x = x_m;
            tgt.y = y_m;
            tgt.distance = dist_m;
            tgt.angle = angle_deg;
            tgt.speed = speed_ms;
            tgt.resolution = rawR;
            tgt.sensorSource = 2; // HLK-LD6003D (60 GHz)
            tgt.confidence = 95;
            tgt.lastSeenMs = millis();
            updateTrail(tgt, x_m, y_m);
            validCount++;
        } else {
            tgt.valid = false;
        }
    }

    _activeCount = validCount;
}

void LD6003DDriver::parseSYFrame(const uint8_t* frame, size_t length) {
    char hexBuffer[70];
    snprintf(hexBuffer, sizeof(hexBuffer),
        "SY %02X %02X | LEN:%d | %02X %02X",
        frame[2], frame[3], (int)length,
        frame[length - 2], frame[length - 1]);
    _latestRawHex = String(hexBuffer);

    // In SY protocol, payload starts at byte 4
    // Format: [53 59] [Cmd] [Len] [TargetCount] [T1: ID, X_L, X_H, Y_L, Y_H, Spd_L, Spd_H] ... [CKS] [54 43]
    if (length < 8) return;

    uint8_t reportedTargets = frame[4];
    if (reportedTargets > MAX_TARGETS_PER_SENSOR) reportedTargets = MAX_TARGETS_PER_SENSOR;

    uint8_t validCount = 0;
    size_t offset = 5;

    for (uint8_t i = 0; i < reportedTargets; i++) {
        if (offset + 6 >= length) break;

        uint8_t tid = frame[offset];
        int16_t x_cm = (int16_t)(frame[offset + 1] | (frame[offset + 2] << 8));
        int16_t y_cm = (int16_t)(frame[offset + 3] | (frame[offset + 4] << 8));
        int16_t s_cm = (int16_t)(frame[offset + 5] | (frame[offset + 6] << 8));

        float x_m = (float)x_cm / 100.0f;
        float y_m = (float)y_cm / 100.0f;
        float speed_ms = (float)s_cm / 100.0f;

        float dist_m = sqrtf((x_m * x_m) + (y_m * y_m));
        float angle_deg = atan2f(x_m, y_m) * (180.0f / M_PI);

        Target& tgt = _targets[i];
        tgt.id = tid > 0 ? tid : (i + 1);

        if (dist_m > 0.10f && dist_m < 8.5f && y_m > 0.05f) {
            tgt.valid = true;
            tgt.x = x_m;
            tgt.y = y_m;
            tgt.distance = dist_m;
            tgt.angle = angle_deg;
            tgt.speed = speed_ms;
            tgt.resolution = 100;
            tgt.sensorSource = 2; // HLK-LD6003D (60 GHz)
            tgt.confidence = 95;
            tgt.lastSeenMs = millis();
            updateTrail(tgt, x_m, y_m);
            validCount++;
        } else {
            tgt.valid = false;
        }

        offset += 7;
    }

    _activeCount = validCount;
}

void LD6003DDriver::updateTrail(Target& target, float x, float y) {
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
