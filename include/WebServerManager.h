#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "SensorFusion.h"
#include "LD2452.h"
#include "LD6003D.h"

class WebServerManager {
public:
    WebServerManager(SensorFusionEngine& fusion, SystemConfig& config, 
                     LD2452Driver& radar24, LD6003DDriver& radar60);
    void begin();
    void update();

    String getIPAddress() const { return _ipAddress; }
    bool isConnectedSTA() const { return _connectedSTA; }

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
    SensorFusionEngine& _fusion;
    SystemConfig& _config;
    LD2452Driver& _radar24;
    LD6003DDriver& _radar60;

    String _ipAddress;
    bool _connectedSTA;
    uint32_t _lastBroadcastTime;

    void setupWiFi();
    void setupRoutes();
    void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                         AwsEventType type, void* arg, uint8_t* data, size_t len);
    void handleWebSocketMessage(void* arg, uint8_t* data, size_t len);
    void broadcastTelemetry();
};
