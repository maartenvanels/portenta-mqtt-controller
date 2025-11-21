#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <Arduino.h>

#ifdef USE_ETHERNET
    #include <PortentaEthernet.h>
    #include <Ethernet.h>
    #include <EthernetClient.h>
#else
    #include <WiFi.h>
    #include <WiFiClient.h>
#endif

/**
 * @brief Thin wrapper around WiFiClient/EthernetClient for unified interface
 *
 * Solves the Single Responsibility Principle violation by providing a single
 * abstraction for network clients regardless of underlying transport.
 * This eliminates massive code duplication in WebServer (46 lines x 2).
 */
class NetworkClient {
public:
    NetworkClient() = default;

#ifdef USE_ETHERNET
    NetworkClient(EthernetClient& client) : ethernetClient_(&client) {}

    operator bool() const { return ethernetClient_ && *ethernetClient_; }
    bool connected() { return ethernetClient_ && ethernetClient_->connected(); }
    int available() { return ethernetClient_ ? ethernetClient_->available() : 0; }
    int read() { return ethernetClient_ ? ethernetClient_->read() : -1; }
    int read(uint8_t *buf, size_t size) { return ethernetClient_ ? ethernetClient_->read(buf, size) : 0; }
    size_t write(uint8_t b) { return ethernetClient_ ? ethernetClient_->write(b) : 0; }
    size_t write(const uint8_t *buf, size_t size) { return ethernetClient_ ? ethernetClient_->write(buf, size) : 0; }
    size_t print(const char* str) { return ethernetClient_ ? ethernetClient_->print(str) : 0; }
    size_t print(const String& str) { return ethernetClient_ ? ethernetClient_->print(str) : 0; }
    size_t println(const char* str) { return ethernetClient_ ? ethernetClient_->println(str) : 0; }
    size_t println(const String& str) { return ethernetClient_ ? ethernetClient_->println(str) : 0; }
    size_t println() { return ethernetClient_ ? ethernetClient_->println() : 0; }
    void stop() { if (ethernetClient_) ethernetClient_->stop(); }
    void flush() { if (ethernetClient_) ethernetClient_->flush(); }
    String readStringUntil(char terminator) {
        return ethernetClient_ ? ethernetClient_->readStringUntil(terminator) : String("");
    }

    EthernetClient* get() { return ethernetClient_; }

private:
    EthernetClient* ethernetClient_ = nullptr;

#else
    NetworkClient(WiFiClient& client) : wifiClient_(&client) {}

    operator bool() const { return wifiClient_ && *wifiClient_; }
    bool connected() { return wifiClient_ && wifiClient_->connected(); }
    int available() { return wifiClient_ ? wifiClient_->available() : 0; }
    int read() { return wifiClient_ ? wifiClient_->read() : -1; }
    int read(uint8_t *buf, size_t size) { return wifiClient_ ? wifiClient_->read(buf, size) : 0; }
    size_t write(uint8_t b) { return wifiClient_ ? wifiClient_->write(b) : 0; }
    size_t write(const uint8_t *buf, size_t size) { return wifiClient_ ? wifiClient_->write(buf, size) : 0; }
    size_t print(const char* str) { return wifiClient_ ? wifiClient_->print(str) : 0; }
    size_t print(const String& str) { return wifiClient_ ? wifiClient_->print(str) : 0; }
    size_t println(const char* str) { return wifiClient_ ? wifiClient_->println(str) : 0; }
    size_t println(const String& str) { return wifiClient_ ? wifiClient_->println(str) : 0; }
    size_t println() { return wifiClient_ ? wifiClient_->println() : 0; }
    void stop() { if (wifiClient_) wifiClient_->stop(); }
    void flush() { if (wifiClient_) wifiClient_->flush(); }
    String readStringUntil(char terminator) {
        return wifiClient_ ? wifiClient_->readStringUntil(terminator) : String("");
    }

    WiFiClient* get() { return wifiClient_; }

private:
    WiFiClient* wifiClient_ = nullptr;
#endif
};

#endif // NETWORK_CLIENT_H
