#ifndef NETWORKSETTINGS_H
#define NETWORKSETTINGS_H

#include <Arduino.h>
#include <ArduinoJson.h>

class NetworkSettings {
public:
    struct WiFiConfig {
        String ssid;
        String password;
        String apSsid;
        String apPassword;
        bool useAP;
    };

    struct MQTTConfig {
        String broker;
        uint16_t port;
        String username;
        String password;
        String clientId;
    };

    struct NTPConfig {
        String server;
        long timeOffset; // in seconds
        unsigned long updateInterval; // in milliseconds
    };

    static NetworkSettings& getInstance();

    // Load/Save settings
    bool loadSettings();
    bool saveSettings();
    bool loadFromJson(const String& json);
    String toJson();

    // Getters
    const WiFiConfig& getWiFiConfig() const { return wifiConfig_; }
    const MQTTConfig& getMQTTConfig() const { return mqttConfig_; }
    const NTPConfig& getNTPConfig() const { return ntpConfig_; }

    // Setters
    void setWiFiConfig(const WiFiConfig& config);
    void setMQTTConfig(const MQTTConfig& config);
    void setNTPConfig(const NTPConfig& config);

    // Check if settings are configured
    bool isConfigured() const;

private:
    NetworkSettings();
    ~NetworkSettings() = default;

    // Prevent copying
    NetworkSettings(const NetworkSettings&) = delete;
    NetworkSettings& operator=(const NetworkSettings&) = delete;

    WiFiConfig wifiConfig_;
    MQTTConfig mqttConfig_;
    NTPConfig ntpConfig_;

    void setDefaults();
    bool loadFromFlash();
    bool saveToFlash();
};

#endif // NETWORKSETTINGS_H
