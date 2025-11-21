#ifndef ANALOG_INPUT_HANDLER_H
#define ANALOG_INPUT_HANDLER_H

#include "IO/BasePinHandler.h"

namespace IO {

class AnalogInputHandler : public BasePinHandler {
public:
    AnalogInputHandler() = default;
    virtual ~AnalogInputHandler() = default;
    
    virtual bool initialize(const PinConfiguration& config) override;
    virtual void process() override;
    virtual bool setValue(float value) override;
    virtual PinState getState() const override { return state_; }
    virtual bool updateConfiguration(const PinConfiguration& config) override;
    
private:
    bool isCurrentMode_ = false;  // false = voltage mode (0-10V), true = current mode (4-20mA)
    float filterAlpha_ = 0.1f;    // Low-pass filter coefficient
    float filteredValue_ = 0.0f;  // Filtered analog value
};

} // namespace IO

#endif // ANALOG_INPUT_HANDLER_H
