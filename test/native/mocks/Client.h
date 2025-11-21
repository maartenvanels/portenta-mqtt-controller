#pragma once
#include "Arduino.h"

class Client : public Stream {
public:
    virtual int connect(IPAddress ip, uint16_t port) { return 1; }
    virtual int connect(const char *host, uint16_t port) { return 1; }
    virtual size_t write(uint8_t) { return 1; }
    virtual size_t write(const uint8_t *buf, size_t size) { return size; }
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int read(uint8_t *buf, size_t size) { return 0; }
    virtual int peek() { return -1; }
    virtual void flush() {}
    virtual void stop() {}
    virtual uint8_t connected() { return 1; }
    virtual operator bool() { return true; }
};
