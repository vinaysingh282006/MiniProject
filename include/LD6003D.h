#pragma once
#include <Arduino.h>
#include "Config.h"

class LD6003DDriver {
public:
    LD6003DDriver();
    void begin(HardwareSerial& serialPort, int rxPin, int txPin, uint32_t baudRate);
    void update();

    // Target access
    uint8_t getActiveTargetCount() const { return _activeCount; }
    const Target& getTarget(uint8_t index) const { return _targets[index]; }
    bool isConnected() const { return _connected; }
    uint32_t getPacketCount() const { return _packetCount; }

    // Raw hex debug inspection
    String getLatestRawHex() const { return _latestRawHex; }

private:
    HardwareSerial* _serial;
    Target _targets[MAX_TARGETS_PER_SENSOR];
    uint8_t _activeCount;
    bool _connected;
    uint32_t _lastPacketTime;
    uint32_t _packetCount;
    String _latestRawHex;

    // Buffer for parsing
    static const size_t MAX_BUF_LEN = 64;
    uint8_t _rxBuffer[MAX_BUF_LEN];
    size_t _rxIndex;

    void parseStandardFrame(const uint8_t* frame, size_t length);
    void parseSYFrame(const uint8_t* frame, size_t length);
    void updateTrail(Target& target, float x, float y);
};
