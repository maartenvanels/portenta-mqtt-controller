#ifndef BASE_PIN_HANDLER_H
#define BASE_PIN_HANDLER_H

#include "IO/IPinHandler.h"
#include "Hardware/MachineControlHAL.h"

namespace IO {

class BasePinHandler : public IPinHandler {
public:
    BasePinHandler() = default;
    virtual ~BasePinHandler() = default;
    
    // Common implementations
    virtual bool initialize(const PinConfiguration& config) override;
    virtual void shutdown() override;
    virtual PinConfiguration getConfiguration() const override { return config_; }
    virtual void setStateChangeCallback(StateChangeCallback callback) override { callback_ = callback; }
    virtual bool isHealthy() const override { return healthy_; }
    virtual std::string getErrorMessage() const override { return errorMessage_; }
    
    // Common JSON serialization
    virtual ArduinoJson::DynamicJsonDocument toJson() const override;
    virtual bool fromJson(const ArduinoJson::JsonVariantConst& doc) override;
    
protected:
    // Helper methods for derived classes
    void updateState(float value);
    bool shouldSample() const;
    void setError(const std::string& message);
    void clearError();
    
    // Member variables
    PinConfiguration config_;
    PinState state_;
    StateChangeCallback callback_;
    HAL::MachineControlHAL& hal_ = HAL::MachineControlHAL::getInstance();
    
    bool initialized_ = false;
    bool healthy_ = true;
    std::string errorMessage_;
    uint32_t lastSampleTime_ = 0;
    float lastNotifiedValue_ = 0.0f;
    
    // Debouncing for digital inputs
    uint32_t lastDebounceTime_ = 0;
    bool lastDebounceState_ = false;
    bool stableState_ = false;
};

} // namespace IO

#endif // BASE_PIN_HANDLER_H
