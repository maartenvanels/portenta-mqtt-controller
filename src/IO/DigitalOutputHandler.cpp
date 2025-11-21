#include "IO/DigitalOutputHandler.h"

namespace IO {

bool DigitalOutputHandler::initialize(const PinConfiguration& config) {
    if (!BasePinHandler::initialize(config)) {
        return false;
    }
    
    // Validate pin number for digital outputs (0-7)
    if (config_.pinNumber > 7) {
        setError("Invalid digital output pin number. Must be 0-7.");
        return false;
    }
    
    // Set initial state
    currentState_ = false;
    hal_.setDigitalOutput(config_.pinNumber, currentState_);
    updateState(currentState_ ? 1.0f : 0.0f);
    
    return true;
}

void DigitalOutputHandler::process() {
    // Digital outputs are write-only and don't need continuous processing
    // The state is maintained in currentState_ and updated via setValue()
    // No need to read back from hardware as outputs don't change by themselves

    // Note: If we needed to read back output state, we should use a dedicated
    // readDigitalOutput() function, NOT readDigitalInput() which reads the
    // separate digital INPUT pins that happen to have the same pin numbers!
}

bool DigitalOutputHandler::setValue(float value) {
    if (!initialized_ || !healthy_) {
        return false;
    }
    
    // Convert float to boolean (>0.5 = true)
    bool newState = (value > 0.5f);
    
    // Apply invert logic if configured
    if (config_.invertLogic) {
        newState = !newState;
    }
    
    // Set the hardware output
    if (hal_.setDigitalOutput(config_.pinNumber, newState)) {
        currentState_ = newState;
        updateState(config_.invertLogic ? !currentState_ : currentState_ ? 1.0f : 0.0f);
        return true;
    }
    
    setError("Failed to set digital output");
    return false;
}

bool DigitalOutputHandler::updateConfiguration(const PinConfiguration& config) {
    if (config.type != PinType::DIGITAL_OUTPUT) {
        return false;
    }
    
    config_ = config;
    return true;
}

} // namespace IO