#pragma once
#include <Arduino.h>
#include "Config.h"

class LD2452Driver {
public:
    LD2452Driver();
    void begin(HardwareSerial& serialPort, int rxPin, int txPin, uint32_t baudRate);
    void update();

    // Target access
    uint8_t getActiveTargetCount() const { return _activeCount; }
    const Target& getTarget(uint8_t index) const { return _targets[index]; }
    bool isConnected() const { return _connected; }
    uint32_t getPacketCount() const { return _packetCount; }
    
    // Raw packet debug stream for Web and Serial monitor
    String getLatestRawHex() const { return _latestRawHex; }

private:
    HardwareSerial* _serial;
    Target _targets[MAX_TARGETS_PER_SENSOR];
    uint8_t _activeCount;
    bool _connected;
    uint32_t _lastPacketTime;
    uint32_t _packetCount;
    String _latestRawHex;

    // Packet parsing state machine
    static const size_t FRAME_LEN = 30;
    uint8_t _rxBuffer[FRAME_LEN];
    size_t _rxIndex;

    void parseFrame(const uint8_t* frame);
    void updateTrail(Target& target, float x, float y);
};
