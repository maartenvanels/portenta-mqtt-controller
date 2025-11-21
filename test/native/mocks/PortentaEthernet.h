#pragma once
#include "Arduino.h"
#include "Client.h"

class EthernetClass {
public:
    void begin(uint8_t* mac, IPAddress ip) {}
    int maintain() { return 0; }
    IPAddress localIP() { return IPAddress(0,0,0,0); }
};

class EthernetClient : public Client {
public:
    EthernetClient() {}
    int connect(IPAddress ip, uint16_t port) { return 0; }
    int connect(const char* host, uint16_t port) { return 0; }
    size_t write(uint8_t b) { return 0; }
    size_t write(const uint8_t *buf, size_t size) { return 0; }
    int available() { return 0; }
    int read() { return 0; }
    int read(uint8_t *buf, size_t size) { return 0; }
    int peek() { return 0; }
    void flush() {}
    void stop() {}
    uint8_t connected() { return 0; }
    operator bool() { return false; }
};

extern EthernetClass Ethernet;
