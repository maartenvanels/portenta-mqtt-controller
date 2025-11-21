#include "IO/DigitalInputHandler.h"
#include <Arduino.h>

namespace IO {

bool DigitalInputHandler::initialize(const PinConfiguration& config) {
    if (!BasePinHandler::initialize(config)) {
        return false;
    }

    // Validate pin number for digital inputs (0-7)
    if (config_.pinNumber > 7) {
        setError("Invalid digital input pin number. Must be 0-7.");
        return false;
    }

    // Initialize state - will be read in first process() call
    lastInputState_ = false;
    updateState(0.0f);

    Serial.print("Digital Input initialized on channel ");
    Serial.println(config.pinNumber);
    return true;
}

bool DigitalInputHandler::setValue(float value) {
    Serial.println("ERROR: Cannot write to digital input pin");
    return false;
}

void DigitalInputHandler::process() {
    // Check for state changes at configured sample rate
    if (!shouldSample()) {
        return;
    }

    lastSampleTime_ = millis();

    bool currentState = hal_.readDigitalInput(config_.pinNumber);

    // Update state on first read (when not valid yet) or when state changed
    if (!state_.isValid || currentState != lastInputState_) {
        lastInputState_ = currentState;
        updateState(currentState ? 1.0f : 0.0f);
    }
}

bool DigitalInputHandler::updateConfiguration(const PinConfiguration& config) {
    // For digital inputs, only allow updating sample rate and other non-critical params
    config_.sampleRateMs = config.sampleRateMs;
    config_.scaleFactor = config.scaleFactor;
    config_.offset = config.offset;
    config_.name = config.name;
    config_.mqttTopic = config.mqttTopic;

    return true;
}

} // namespace IO
