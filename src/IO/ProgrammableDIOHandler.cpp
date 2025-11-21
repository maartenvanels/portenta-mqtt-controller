#include "IO/ProgrammableDIOHandler.h"
#include <Arduino.h>

namespace IO {

bool ProgrammableDIOHandler::initialize(const PinConfiguration& config) {
    if (!BasePinHandler::initialize(config)) {
        return false;
    }

    // Validate pin number for programmable DIO (0-11)
    if (config_.pinNumber > 11) {
        setError("Invalid programmable DIO pin number. Must be 0-11.");
        return false;
    }

    isOutput_ = (config.mode == PinMode::OUTPUT);
    lastInputState_ = false;

    // Configure programmable digital I/O via HAL (sets initial state for outputs)
    hal_.configureProgrammableIO(config.pinNumber, isOutput_);

    // Initialize state
    updateState(0.0f);

    if (isOutput_) {
        Serial.print("Programmable DIO Output initialized on channel ");
    } else {
        Serial.print("Programmable DIO Input initialized on channel ");
    }

    Serial.println(config.pinNumber);
    return true;
}

bool ProgrammableDIOHandler::setValue(float value) {
    if (!isOutput_) {
        Serial.println("ERROR: Cannot write to input pin");
        return false;
    }

    bool newState = (value > 0.5f);

    if (!hal_.setProgrammableOutput(config_.pinNumber, newState)) {
        setError("Failed to set programmable output");
        return false;
    }

    updateState(newState ? 1.0f : 0.0f);
    return true;
}

void ProgrammableDIOHandler::process() {
    if (isOutput_) {
        // Outputs don't need periodic processing
        return;
    }

    // For inputs, check for state changes at configured sample rate
    if (!shouldSample()) {
        return;
    }

    lastSampleTime_ = millis();

    bool currentState = hal_.readProgrammableInput(config_.pinNumber);

    // Update state on first read (when not valid yet) or when state changed
    if (!state_.isValid || currentState != lastInputState_) {
        lastInputState_ = currentState;
        updateState(currentState ? 1.0f : 0.0f);
    }
}

bool ProgrammableDIOHandler::updateConfiguration(const PinConfiguration& config) {
    // For programmable DIO, only allow updating sample rate and other non-critical params
    config_.sampleRateMs = config.sampleRateMs;
    config_.scaleFactor = config.scaleFactor;
    config_.offset = config.offset;
    config_.name = config.name;
    config_.mqttTopic = config.mqttTopic;

    return true;
}

} // namespace IO
