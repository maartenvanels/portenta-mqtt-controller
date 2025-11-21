#include "IO/AnalogOutputHandler.h"

namespace IO {

bool AnalogOutputHandler::initialize(const PinConfiguration& config) {
    if (!BasePinHandler::initialize(config)) {
        return false;
    }
    
    // Validate pin number for analog outputs (0-3)
    if (config_.pinNumber > 3) {
        setError("Invalid analog output pin number. Must be 0-3.");
        return false;
    }
    
    // Set initial value to 0
    setValue(0.0f);
    
    return true;
}

void AnalogOutputHandler::process() {
    // Nothing to process for analog output unless we want to implement ramping or timeouts
    // Just update last sample time to keep it "active"
    if (shouldSample()) {
        lastSampleTime_ = millis();
    }
}

bool AnalogOutputHandler::setValue(float value) {
    if (!initialized_) {
        return false;
    }
    
    // Apply scale/offset (inverse of input)
    // value = (raw * scale) + offset  =>  raw = (value - offset) / scale
    float rawValue = value;
    if (config_.scaleFactor != 0.0f) {
        rawValue = (value - config_.offset) / config_.scaleFactor;
    }
    
    if (hal_.setAnalogOutput(config_.pinNumber, rawValue)) {
        updateState(value); // Update state with the logical value
        return true;
    }
    
    setError("Failed to set analog output");
    return false;
}

bool AnalogOutputHandler::updateConfiguration(const PinConfiguration& config) {
    if (config.type != PinType::ANALOG_OUTPUT_VOLTAGE && config.type != PinType::ANALOG_OUTPUT_CURRENT) {
        return false;
    }
    
    config_ = config;
    return true;
}

} // namespace IO
