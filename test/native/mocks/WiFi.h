#pragma once
#include "Arduino.h"
#include "Client.h"

#pragma once
#include "Arduino.h"
#include "Client.h"

// WiFi status constants
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6

class WiFiClass {
public:
    void begin(const char* ssid, const char* password) {}
    int status() { return WL_CONNECTED; }
    IPAddress localIP() { return IPAddress(192, 168, 1, 100); }
    int RSSI() { return -50; }
    void disconnect() {}
};

extern WiFiClass WiFi;
