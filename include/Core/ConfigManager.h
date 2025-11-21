#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <ArduinoJson.h>
#include <string>
#include <vector>
#include "IO/IPinHandler.h"

class ConfigManager
{
public:
    static ConfigManager &getInstance();

    // Load configuration
    bool loadDefaultConfig();                    // From embedded config.json
    bool loadFromStorage();                      // From flash memory
    bool loadFromJson(const String &jsonString); // From MQTT update

    // Save configuration
    bool saveToStorage(); // Save to flash

    // Factory reset
    bool factoryReset(); // Reset to embedded defaults and save to QSPI

    // Version management
    uint32_t getConfigVersion() const;   // Get version of current config
    uint32_t getEmbeddedVersion() const; // Get version of embedded config

    // Get pins configuration
    const std::vector<IO::PinConfiguration> &getPins() const;

    // Get device name
    String getDeviceName() const;

    // Serialize current config
    String toJson() const;

    // Validation
    bool validate() const;

    // Reload trigger
    bool shouldReload() const { return shouldReload_; }
    void setShouldReload(bool reload) { shouldReload_ = reload; }

private:
    ConfigManager() : shouldReload_(false), configDoc_(16384) {}

    bool shouldReload_;
    ArduinoJson::DynamicJsonDocument configDoc_;
    std::string deviceName_;
    std::vector<IO::PinConfiguration> pins_;

    // Helper methods
    bool parseJsonConfig(ArduinoJson::JsonDocument &doc);
    IO::PinType stringToPinType(const String &typeStr);
    IO::PinMode stringToPinMode(const String &modeStr);
};

#endif // CONFIG_MANAGER_H
