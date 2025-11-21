#ifndef IO_CONTROLLER_H
#define IO_CONTROLLER_H

#include "IO/IPinHandler.h"
#include <map>
#include <memory>
#include <vector>
#include <functional>

namespace IO {

class IoController {
public:
    static IoController& getInstance() {
        static IoController instance;
        return instance;
    }
    
    // Initialization
    bool initialize();
    void shutdown();
    
    // Pin management
    bool addPin(const PinConfiguration& config);
    bool removePin(uint8_t pinNumber);
    bool updatePin(uint8_t pinNumber, const PinConfiguration& config);
    
    // Pin access
    IPinHandler* getPin(uint8_t pinNumber);  // Get by pin number only (legacy, may return wrong pin if multiple types)
    IPinHandler* getPin(uint16_t compositeKey);  // Get by composite key (type << 8 | pinNumber)
    IPinHandler* getPinByName(const std::string& name);
    IPinHandler* getPinByTopic(const std::string& topic);
    std::vector<uint16_t> getAllPinNumbers() const;  // Returns composite keys (type << 8 | pinNumber)
    
    // Processing
    void process();
    
    // State management
    bool setPinValue(uint8_t pinNumber, float value);
    PinState getPinState(uint8_t pinNumber) const;
    ArduinoJson::DynamicJsonDocument getAllPinStates() const;
    
    // Configuration
    bool loadConfiguration(const ArduinoJson::JsonVariantConst& config);
    ArduinoJson::DynamicJsonDocument saveConfiguration() const;
    
    // Callbacks
    using GlobalStateChangeCallback = std::function<void(uint8_t pin, const PinState& state, const std::string& topic)>;
    void setGlobalStateChangeCallback(GlobalStateChangeCallback callback);
    
    // Diagnostics
    ArduinoJson::DynamicJsonDocument getDiagnostics() const;
    bool isHealthy() const;
    
private:
    IoController() = default;
    ~IoController() = default;
    
    IoController(const IoController&) = delete;
    IoController& operator=(const IoController&) = delete;
    
    // Pin handler storage - use composite key (type << 8 | pinNumber) to allow same pin numbers for different types
    std::map<uint16_t, std::unique_ptr<IPinHandler>> pinHandlers_;
    std::map<std::string, uint16_t> nameToPin_;
    std::map<std::string, uint16_t> topicToPin_;
    
    // State
    bool initialized_ = false;
    GlobalStateChangeCallback globalCallback_;
    
    // Helper methods
    void handlePinStateChange(uint16_t key, uint8_t pin, const PinState& state);
    bool validatePinNumber(uint8_t pinNumber, PinType type) const;
    void updateMappings(uint16_t key, const PinConfiguration& config);
    void removeMappings(uint16_t key);

    // Create unique key from type and pin number
    static uint16_t makeKey(PinType type, uint8_t pinNumber) {
        return (static_cast<uint16_t>(type) << 8) | pinNumber;
    }
};

} // namespace IO

#endif // IO_CONTROLLER_H
