#include "IO/RTDInputHandler.h"

namespace IO {

bool RTDInputHandler::initialize(const PinConfiguration& config) {
    if (!BasePinHandler::initialize(config)) {
        return false;
    }
    
    // Validate pin number for RTD inputs (0-2)
    if (config_.pinNumber > 2) {
        setError("Invalid RTD input channel. Must be 0-2.");
        return false;
    }
    
    // Check for Fahrenheit flag in configuration
    usesFahrenheit_ = (config_.scaleFactor == 1.8f && config_.offset == 32.0f);
    
    // Initialize with room temperature
    filteredTemp_ = 20.0f;
    
    return true;
}

void RTDInputHandler::process() {
    if (!shouldSample()) {
        return;
    }
    
    lastSampleTime_ = millis();
    
    // Read RTD temperature from hardware (returns Celsius)
    float tempCelsius = hal_.readRTDTemperature(config_.pinNumber);
    
    if (tempCelsius <= -273.0f) {
        setError("Failed to read RTD temperature or probe disconnected");
        return;
    }
    
    // Apply low-pass filter for noise reduction
    filteredTemp_ = (filterAlpha_ * tempCelsius) + ((1.0f - filterAlpha_) * filteredTemp_);
    
    // Convert to Fahrenheit if needed
    float outputTemp = filteredTemp_;
    if (usesFahrenheit_) {
        outputTemp = (filteredTemp_ * 1.8f) + 32.0f;
    }
    
    // Update state (let base class handle scaling if different from F conversion)
    if (!usesFahrenheit_) {
        updateState(outputTemp);
    } else {
        // If using Fahrenheit, bypass the scale/offset in updateState
        state_.rawValue = filteredTemp_;
        state_.value = outputTemp;
        state_.lastUpdateTime = millis();
        state_.changeCount++;
        state_.isValid = true;
        
        if (callback_ && abs(outputTemp - lastNotifiedValue_) > 0.1f) {
            callback_(config_.pinNumber, state_);
            lastNotifiedValue_ = outputTemp;
        }
    }
    
    clearError();
}

bool RTDInputHandler::setValue(float value) {
    // RTD inputs are read-only
    setError("Cannot set value on RTD temperature input");
    return false;
}

bool RTDInputHandler::updateConfiguration(const PinConfiguration& config) {
    if (config.type != PinType::RTD_INPUT) {
        return false;
    }
    
    config_ = config;
    
    // Check for Fahrenheit flag
    usesFahrenheit_ = (config_.scaleFactor == 1.8f && config_.offset == 32.0f);
    
    // Update filter coefficient based on sample rate
    // Temperature changes slowly, so use heavy filtering
    filterAlpha_ = 0.5f / (1.0f + (1000.0f / config_.sampleRateMs));
    
    return true;
}

} // namespace IO