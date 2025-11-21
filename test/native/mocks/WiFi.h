#pragma once
#include "Arduino.h"
#include "Client.h"

class WiFiClass {
public:
    int RSSI() { return -50; }
    int status() { return 3; } // WL_CONNECTED
    IPAddress localIP() { return IPAddress(192, 168, 1, 100); }
};

extern WiFiClass WiFi;
