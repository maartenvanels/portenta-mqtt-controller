#include "NetworkSettings.h"
#include "credentials.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_UnifiedStorage.h>

NetworkSettings& NetworkSettings::getInstance() {
    static NetworkSettings instance;
    return instance;
}

NetworkSettings::NetworkSettings() {
    setDefaults();
    loadSettings();
}

void NetworkSettings::setDefaults() {
    // WiFi defaults from credentials.h
    wifiConfig_.ssid = WIFI_SSID;
    wifiConfig_.password = WIFI_PASSWORD;
    wifiConfig_.apSsid = "Portenta-Setup";
    wifiConfig_.apPassword = "portenta123";
    wifiConfig_.useAP = false;

    // MQTT defaults from credentials.h
    mqttConfig_.broker = MQTT_BROKER;
    mqttConfig_.port = MQTT_PORT;
    mqttConfig_.username = MQTT_USERNAME;
    mqttConfig_.password = MQTT_PASSWORD;
    mqttConfig_.clientId = MQTT_CLIENT_ID;

    // NTP defaults
    ntpConfig_.server = "pool.ntp.org";
    ntpConfig_.timeOffset = 0; // UTC
    ntpConfig_.updateInterval = 3600000; // 1 hour
}

bool NetworkSettings::loadSettings() {
    Serial.println("\n=== Loading Network Settings ===");

    if (!loadFromFlash()) {
        Serial.println("No saved settings found, using defaults");
        return false;
    }

    Serial.println("Settings loaded successfully");
    Serial.print("WiFi SSID: ");
    Serial.println(wifiConfig_.ssid);
    Serial.print("MQTT Broker: ");
    Serial.println(mqttConfig_.broker);
    return true;
}

bool NetworkSettings::saveSettings() {
    Serial.println("\n=== Saving Network Settings ===");

    if (!saveToFlash()) {
        Serial.println("Failed to save settings");
        return false;
    }

    Serial.println("Settings saved successfully");
    return true;
}

bool NetworkSettings::loadFromJson(const String& json) {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return false;
    }

    // Parse WiFi config
    if (doc.containsKey("wifi")) {
        JsonObject wifi = doc["wifi"];
        wifiConfig_.ssid = wifi["ssid"] | wifiConfig_.ssid;
        
        String newPass = wifi["password"] | "";
        if (newPass.length() > 0 && newPass != "********") {
            wifiConfig_.password = newPass;
        }

        wifiConfig_.apSsid = wifi["apSsid"] | wifiConfig_.apSsid;
        
        String newApPass = wifi["apPassword"] | "";
        if (newApPass.length() > 0 && newApPass != "********") {
            wifiConfig_.apPassword = newApPass;
        }
        
        wifiConfig_.useAP = wifi["useAP"] | wifiConfig_.useAP;
    }

    // Parse MQTT config
    if (doc.containsKey("mqtt")) {
        JsonObject mqtt = doc["mqtt"];
        mqttConfig_.broker = mqtt["broker"] | mqttConfig_.broker;
        mqttConfig_.port = mqtt["port"] | mqttConfig_.port;
        mqttConfig_.username = mqtt["username"] | mqttConfig_.username;
        
        String newMqttPass = mqtt["password"] | "";
        if (newMqttPass.length() > 0 && newMqttPass != "********") {
            mqttConfig_.password = newMqttPass;
        }
        
        mqttConfig_.clientId = mqtt["clientId"] | mqttConfig_.clientId;
    }

    // Parse NTP config
    if (doc.containsKey("ntp")) {
        JsonObject ntp = doc["ntp"];
        ntpConfig_.server = ntp["server"] | ntpConfig_.server;
        ntpConfig_.timeOffset = ntp["timeOffset"] | ntpConfig_.timeOffset;
        ntpConfig_.updateInterval = ntp["updateInterval"] | ntpConfig_.updateInterval;
    }

    return true;
}

String NetworkSettings::toJson() {
    DynamicJsonDocument doc(2048);

    // WiFi config
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["ssid"] = wifiConfig_.ssid;
    wifi["password"] = wifiConfig_.password.length() > 0 ? "********" : "";  // Hide password
    wifi["apSsid"] = wifiConfig_.apSsid;
    wifi["apPassword"] = wifiConfig_.apPassword.length() > 0 ? "********" : "";
    wifi["useAP"] = wifiConfig_.useAP;

    // MQTT config
    JsonObject mqtt = doc.createNestedObject("mqtt");
    mqtt["broker"] = mqttConfig_.broker;
    mqtt["port"] = mqttConfig_.port;
    mqtt["username"] = mqttConfig_.username;
    mqtt["password"] = mqttConfig_.password.length() > 0 ? "********" : "";  // Hide password
    mqtt["clientId"] = mqttConfig_.clientId;

    // NTP config
    JsonObject ntp = doc.createNestedObject("ntp");
    ntp["server"] = ntpConfig_.server;
    ntp["timeOffset"] = ntpConfig_.timeOffset;
    ntp["updateInterval"] = ntpConfig_.updateInterval;

    String result;
    serializeJson(doc, result);
    return result;
}

void NetworkSettings::setWiFiConfig(const WiFiConfig& config) {
    wifiConfig_ = config;
}

void NetworkSettings::setMQTTConfig(const MQTTConfig& config) {
    mqttConfig_ = config;
}

void NetworkSettings::setNTPConfig(const NTPConfig& config) {
    ntpConfig_ = config;
}

bool NetworkSettings::isConfigured() const {
    // Settings are configured if WiFi SSID and MQTT broker are not empty
    return !wifiConfig_.ssid.isEmpty() && !mqttConfig_.broker.isEmpty();
}

bool NetworkSettings::loadFromFlash() {
    // Use Partition 4 (User Data) - same as Logger
    InternalStorage storage(3, "user", FS_LITTLEFS);

    if (!storage.begin()) {
        Serial.println("Cannot access user partition for settings");
        return false;
    }

    // Try to open settings file
    auto root = storage.getRootFolder();
    auto file = root.createFile("settings.json", FileMode::READ);

    if (!file.exists()) {
        Serial.println("No settings.json found in flash storage");
        return false;
    }

    // Read file content
    size_t fileSize = file.available();
    if (fileSize == 0 || fileSize > 4096) {
        Serial.println("Invalid settings file size");
        file.close();
        return false;
    }

    uint8_t* buffer = new uint8_t[fileSize + 1];
    size_t bytesRead = file.read(buffer, fileSize);
    buffer[bytesRead] = '\0';
    file.close();

    // Parse JSON
    bool success = loadFromJson(String((char*)buffer));
    delete[] buffer;

    if (success) {
        Serial.println("Settings loaded from QSPI flash");
    }

    return success;
}

bool NetworkSettings::saveToFlash() {
    // Use Partition 4 (User Data)
    InternalStorage storage(3, "user", FS_LITTLEFS);

    if (!storage.begin()) {
        Serial.println("Cannot access user partition for settings");

        // Try to format if needed
        if (storage.format(FS_LITTLEFS) && storage.begin()) {
            Serial.println("User partition formatted successfully");
        } else {
            return false;
        }
    }

    // Serialize settings to JSON
    String json = toJson();

    // Write to file
    auto root = storage.getRootFolder();
    auto file = root.createFile("settings.json", FileMode::WRITE);

    if (!file.exists()) {
        Serial.println("Failed to create settings.json");
        return false;
    }

    size_t bytesWritten = file.write((const uint8_t*)json.c_str(), json.length());
    file.close();

    if (bytesWritten == json.length()) {
        Serial.println("Settings saved to QSPI flash");
        return true;
    } else {
        Serial.println("Failed to write all settings data");
        return false;
    }
}
