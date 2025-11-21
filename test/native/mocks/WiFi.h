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

class WiFiClient : public Client {
public:
    WiFiClient() {}
    int connect(IPAddress ip, uint16_t port) { return 1; }
    int connect(const char* host, uint16_t port) { return 1; }
    size_t write(uint8_t b) { return 1; }
    size_t write(const uint8_t *buf, size_t size) { return size; }
    int available() { return 0; }
    int read() { return -1; }
    int read(uint8_t *buf, size_t size) { return 0; }
    int peek() { return -1; }
    void flush() {}
    void stop() {}
    uint8_t connected() { return 1; }
    operator bool() { return true; }
};

extern WiFiClass WiFi;
