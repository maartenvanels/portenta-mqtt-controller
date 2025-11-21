#include "io/BasePinHandler.h"
#include <Arduino.h>
#undef abs

namespace IO {

bool BasePinHandler::initialize(const PinConfiguration& config) {
    config_ = config;
    state_.value = 0.0f;
    state_.rawValue = 0.0f;
    state_.isValid = false;
    state_.lastUpdateTime = 0;
    state_.changeCount = 0;
    
    initialized_ = true;
    healthy_ = true;
    errorMessage_.clear();
    
    return true;
}

void BasePinHandler::shutdown() {
    initialized_ = false;
    state_.isValid = false;
}

void BasePinHandler::updateState(float value) {
    float scaledValue = (value * config_.scaleFactor) + config_.offset;
    
    // Apply min/max constraints
    if (config_.maxValue > config_.minValue) {
        scaledValue = constrain(scaledValue, config_.minValue, config_.maxValue);
    }
    
    // Check if value has changed significantly
    bool valueChanged = (abs(scaledValue - state_.value) > 0.001f);
    bool firstUpdate = !state_.isValid;
    
    if (valueChanged || firstUpdate) {
        state_.rawValue = value;
        state_.value = scaledValue;
        state_.lastUpdateTime = millis();
        state_.isValid = true;  // Always mark as valid after first update
        
        if (valueChanged) {
            state_.changeCount++;
        }
        
        // Notify callback on first update or if value changed enough
        if (callback_ && (firstUpdate || abs(scaledValue - lastNotifiedValue_) > 0.01f)) {
            callback_(config_.pinNumber, state_);
            lastNotifiedValue_ = scaledValue;
        }
    }
}

bool BasePinHandler::shouldSample() const {
    if (!initialized_ || !healthy_) {
        return false;
    }
    
    uint32_t now = millis();
    return (now - lastSampleTime_) >= config_.sampleRateMs;
}

void BasePinHandler::setError(const std::string& message) {
    healthy_ = false;
    errorMessage_ = message;
    state_.isValid = false;
}

void BasePinHandler::clearError() {
    healthy_ = true;
    errorMessage_.clear();
}

ArduinoJson::DynamicJsonDocument BasePinHandler::toJson() const {
    ArduinoJson::DynamicJsonDocument doc(768);

    // Configuration
    ArduinoJson::JsonObject configObj = doc.createNestedObject("config");
    configObj["pin"] = config_.pinNumber;
    configObj["type"] = static_cast<int>(config_.type);
    configObj["mode"] = static_cast<int>(config_.mode);
    configObj["name"] = config_.name;
    configObj["topic"] = config_.mqttTopic;
    configObj["scale"] = config_.scaleFactor;
    configObj["offset"] = config_.offset;
    configObj["debounce"] = config_.debounceMs;
    configObj["sampleRate"] = config_.sampleRateMs;
    configObj["min"] = config_.minValue;
    configObj["max"] = config_.maxValue;
    configObj["invert"] = config_.invertLogic;

    // State
    ArduinoJson::JsonObject stateObj = doc.createNestedObject("state");
    stateObj["value"] = state_.value;
    stateObj["raw"] = state_.rawValue;
    stateObj["valid"] = state_.isValid;
    stateObj["lastUpdate"] = state_.lastUpdateTime;
    stateObj["changes"] = state_.changeCount;

    // Health
    doc["healthy"] = healthy_;
    if (!healthy_) {
        doc["error"] = errorMessage_;
    }

    return doc;
}

bool BasePinHandler::fromJson(const ArduinoJson::JsonVariantConst& doc) {
    if (!doc.containsKey("config")) {
        return false;
    }
    
    ArduinoJson::JsonObjectConst configObj = doc["config"].as<ArduinoJson::JsonObjectConst>();
    if (configObj.isNull()) {
        return false;
    }

    config_.pinNumber = configObj["pin"] | 0;
    config_.type = static_cast<PinType>(configObj["type"] | 0);
    config_.mode = static_cast<PinMode>(configObj["mode"] | 0);

    const char* name = configObj["name"] | "";
    config_.name = name ? name : "";
    const char* topic = configObj["topic"] | "";
    config_.mqttTopic = topic ? topic : "";

    config_.scaleFactor = configObj["scale"] | 1.0f;
    config_.offset = configObj["offset"] | 0.0f;
    config_.debounceMs = configObj["debounce"] | 50u;
    config_.sampleRateMs = configObj["sampleRate"] | 100u;
    config_.minValue = configObj["min"] | 0.0f;
    config_.maxValue = configObj["max"] | 0.0f;
    config_.invertLogic = configObj["invert"] | false;

    return true;
}

} // namespace IO
