#include "IO/IoController.h"
#include "IO/PinHandlerFactory.h"
#include "Hardware/MachineControlHAL.h"
#include <Arduino.h>
#undef abs

namespace IO {

bool IoController::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Initialize the hardware abstraction layer
    if (!HAL::MachineControlHAL::getInstance().initialize()) {
        return false;
    }
    
    // Clear any existing handlers
    pinHandlers_.clear();
    nameToPin_.clear();
    topicToPin_.clear();
    
    initialized_ = true;
    return true;
}

void IoController::shutdown() {
    // Shutdown all pin handlers
    for (auto& entry : pinHandlers_) {
        auto& handler = entry.second;
        if (handler) {
            handler->shutdown();
        }
    }
    
    pinHandlers_.clear();
    nameToPin_.clear();
    topicToPin_.clear();
    
    initialized_ = false;
}

bool IoController::addPin(const PinConfiguration& config) {
    if (!initialized_) {
        Serial.println("  ERROR: IoController not initialized");
        return false;
    }

    // Validate pin number for the given type
    if (!validatePinNumber(config.pinNumber, config.type)) {
        Serial.print("  ERROR: Invalid pin number ");
        Serial.print(config.pinNumber);
        Serial.print(" for type ");
        Serial.println(static_cast<int>(config.type));
        return false;
    }

    // Create unique key for this pin type + number combination
    uint16_t key = makeKey(config.type, config.pinNumber);

    // Check if pin already exists
    if (pinHandlers_.find(key) != pinHandlers_.end()) {
        Serial.print("  ERROR: Pin ");
        Serial.print(config.pinNumber);
        Serial.print(" type ");
        Serial.print(static_cast<int>(config.type));
        Serial.println(" already exists");
        return false;
    }

    // Create the pin handler
    auto handler = PinHandlerFactory::create(config);
    if (!handler) {
        Serial.print("  ERROR: Failed to create handler for pin ");
        Serial.print(config.pinNumber);
        Serial.print(" type ");
        Serial.println(static_cast<int>(config.type));
        return false;
    }

    // Set up state change callback - capture the key to identify the correct handler
    handler->setStateChangeCallback(
        [this, key](uint8_t pin, const PinState& state) {
            handlePinStateChange(key, pin, state);
        }
    );

    // Store the handler
    pinHandlers_[key] = std::move(handler);
    updateMappings(key, config);

    return true;
}

bool IoController::removePin(uint8_t pinNumber) {
    auto it = pinHandlers_.find(pinNumber);
    if (it == pinHandlers_.end()) {
        return false;
    }
    
    // Shutdown the handler
    if (it->second) {
        it->second->shutdown();
    }
    
    // Remove mappings
    removeMappings(pinNumber);
    
    // Remove the handler
    pinHandlers_.erase(it);
    
    return true;
}

bool IoController::updatePin(uint8_t pinNumber, const PinConfiguration& config) {
    auto it = pinHandlers_.find(pinNumber);
    if (it == pinHandlers_.end()) {
        return false;
    }
    
    // Update the handler configuration
    if (!it->second->updateConfiguration(config)) {
        return false;
    }
    
    // Update mappings
    removeMappings(pinNumber);
    updateMappings(pinNumber, config);
    
    return true;
}

IPinHandler* IoController::getPin(uint8_t pinNumber) {
    auto it = pinHandlers_.find(pinNumber);
    return (it != pinHandlers_.end()) ? it->second.get() : nullptr;
}

IPinHandler* IoController::getPin(uint16_t compositeKey) {
    auto it = pinHandlers_.find(compositeKey);
    return (it != pinHandlers_.end()) ? it->second.get() : nullptr;
}

IPinHandler* IoController::getPinByName(const std::string& name) {
    auto it = nameToPin_.find(name);
    if (it != nameToPin_.end()) {
        auto handlerIt = pinHandlers_.find(it->second);
        return (handlerIt != pinHandlers_.end()) ? handlerIt->second.get() : nullptr;
    }
    return nullptr;
}

IPinHandler* IoController::getPinByTopic(const std::string& topic) {
    auto it = topicToPin_.find(topic);
    if (it != topicToPin_.end()) {
        auto handlerIt = pinHandlers_.find(it->second);
        return (handlerIt != pinHandlers_.end()) ? handlerIt->second.get() : nullptr;
    }
    return nullptr;
}

std::vector<uint16_t> IoController::getAllPinNumbers() const {
    std::vector<uint16_t> pins;
    for (const auto& entry : pinHandlers_) {
        pins.push_back(entry.first);  // Return full composite key
    }
    return pins;
}

void IoController::process() {
    if (!initialized_) {
        return;
    }
    
    // Process all pin handlers
    for (auto& entry : pinHandlers_) {
        auto& handler = entry.second;
        if (handler) {
            handler->process();
        }
    }
}

bool IoController::setPinValue(uint8_t pinNumber, float value) {
    auto it = pinHandlers_.find(pinNumber);
    if (it == pinHandlers_.end() || !it->second) {
        return false;
    }
    
    return it->second->setValue(value);
}

PinState IoController::getPinState(uint8_t pinNumber) const {
    auto it = pinHandlers_.find(pinNumber);
    if (it != pinHandlers_.end() && it->second) {
        return it->second->getState();
    }
    
    // Return invalid state
    PinState invalidState;
    invalidState.isValid = false;
    return invalidState;
}

ArduinoJson::DynamicJsonDocument IoController::getAllPinStates() const {
    const size_t capacity = 256 + pinHandlers_.size() * 192;
    ArduinoJson::DynamicJsonDocument doc(capacity);
    ArduinoJson::JsonArray pinsArray = doc.createNestedArray("pins");

    for (const auto& entry : pinHandlers_) {
        const uint8_t pin = entry.first;
        const auto& handler = entry.second;
        if (handler) {
            ArduinoJson::JsonObject pinObj = pinsArray.createNestedObject();
            pinObj["pin"] = pin;
            pinObj["name"] = handler->getConfiguration().name;
            pinObj["topic"] = handler->getConfiguration().mqttTopic;

            const PinState state = handler->getState();
            pinObj["value"] = state.value;
            pinObj["valid"] = state.isValid;
            pinObj["lastUpdate"] = state.lastUpdateTime;
        }
    }

    doc["timestamp"] = millis();
    return doc;
}

bool IoController::loadConfiguration(const ArduinoJson::JsonVariantConst& config) {
    if (!initialized_) {
        return false;
    }
    
    // Clear existing configuration
    shutdown();
    initialize();
    
    // Load pins from configuration
    if (config.containsKey("pins")) {
        ArduinoJson::JsonArrayConst pinsArray = config["pins"].as<ArduinoJson::JsonArrayConst>();
        if (!pinsArray.isNull()) {
            for (ArduinoJson::JsonObjectConst pinObj : pinsArray) {
                PinConfiguration pinConfig{};
                pinConfig.pinNumber = pinObj["pin"] | 0u;
                pinConfig.type = static_cast<PinType>(pinObj["type"] | 0);
                pinConfig.mode = static_cast<PinMode>(pinObj["mode"] | 0);

                const char* name = pinObj["name"] | "";
                pinConfig.name = name ? name : "";
                const char* topic = pinObj["topic"] | "";
                pinConfig.mqttTopic = topic ? topic : "";

                pinConfig.scaleFactor = pinObj["scale"] | 1.0f;
                pinConfig.offset = pinObj["offset"] | 0.0f;
                pinConfig.debounceMs = pinObj["debounce"] | 50u;
                pinConfig.sampleRateMs = pinObj["sampleRate"] | 100u;
                pinConfig.minValue = pinObj["min"] | 0.0f;
                pinConfig.maxValue = pinObj["max"] | 0.0f;
                pinConfig.invertLogic = pinObj["invert"] | false;

                addPin(pinConfig);
            }
        }
    }
    
    return true;
}

ArduinoJson::DynamicJsonDocument IoController::saveConfiguration() const {
    const size_t capacity = 256 + pinHandlers_.size() * 224;
    ArduinoJson::DynamicJsonDocument doc(capacity);
    ArduinoJson::JsonArray pinsArray = doc.createNestedArray("pins");

    for (const auto& entry : pinHandlers_) {
        const auto& handler = entry.second;
        if (handler) {
            auto handlerDoc = handler->toJson();
            ArduinoJson::JsonVariant configVariant = handlerDoc["config"];
            if (!configVariant.isNull()) {
                pinsArray.add(configVariant);
            }
        }
    }
    
    doc["version"] = 1;
    doc["timestamp"] = millis();
    
    return doc;
}

void IoController::setGlobalStateChangeCallback(GlobalStateChangeCallback callback) {
    globalCallback_ = callback;
}

ArduinoJson::DynamicJsonDocument IoController::getDiagnostics() const {
    const size_t capacity = 256 + pinHandlers_.size() * 160;
    ArduinoJson::DynamicJsonDocument doc(capacity);

    doc["initialized"] = initialized_;
    doc["totalPins"] = pinHandlers_.size();
    doc["healthy"] = isHealthy();
    
    ArduinoJson::JsonArray pinsArray = doc.createNestedArray("pins");
    for (const auto& entry : pinHandlers_) {
        const uint8_t pin = entry.first;
        const auto& handler = entry.second;
        if (handler) {
            ArduinoJson::JsonObject pinObj = pinsArray.createNestedObject();
            pinObj["pin"] = pin;
            pinObj["type"] = static_cast<int>(handler->getConfiguration().type);
            pinObj["healthy"] = handler->isHealthy();
            if (!handler->isHealthy()) {
                pinObj["error"] = handler->getErrorMessage();
            }
        }
    }
    
    return doc;
}

bool IoController::isHealthy() const {
    if (!initialized_) {
        return false;
    }
    
    for (const auto& entry : pinHandlers_) {
        const auto& handler = entry.second;
        if (handler && !handler->isHealthy()) {
            return false;
        }
    }
    
    return true;
}

void IoController::handlePinStateChange(uint16_t key, uint8_t pin, const PinState& state) {
    if (globalCallback_) {
        auto it = pinHandlers_.find(key);
        if (it != pinHandlers_.end() && it->second) {
            const auto& config = it->second->getConfiguration();
            globalCallback_(pin, state, config.mqttTopic);
        }
    }
}

bool IoController::validatePinNumber(uint8_t pinNumber, PinType type) const {
    switch (type) {
        case PinType::DIGITAL_OUTPUT:
            return pinNumber <= 7;

        case PinType::DIGITAL_INPUT:
            return pinNumber <= 7;

        case PinType::PROGRAMMABLE_DIO:
            return pinNumber <= 11;

        case PinType::ANALOG_INPUT_VOLTAGE:
        case PinType::ANALOG_INPUT_CURRENT:
        case PinType::RTD_INPUT:
        case PinType::THERMOCOUPLE_INPUT:
            return pinNumber <= 2;

        case PinType::ANALOG_OUTPUT_VOLTAGE:
        case PinType::ANALOG_OUTPUT_CURRENT:
            return pinNumber <= 3;

        case PinType::ENCODER_INPUT:
            return pinNumber <= 1;

        default:
            return false;
    }
}

void IoController::updateMappings(uint16_t key, const PinConfiguration& config) {
    if (!config.name.empty()) {
        nameToPin_[config.name] = key;
    }

    if (!config.mqttTopic.empty()) {
        topicToPin_[config.mqttTopic] = key;
    }
}

void IoController::removeMappings(uint16_t key) {
    auto it = pinHandlers_.find(key);
    if (it != pinHandlers_.end() && it->second) {
        const auto& config = it->second->getConfiguration();

        nameToPin_.erase(config.name);
        topicToPin_.erase(config.mqttTopic);
    }
}

} // namespace IO
