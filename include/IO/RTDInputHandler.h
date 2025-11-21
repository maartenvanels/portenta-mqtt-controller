#ifndef RTD_INPUT_HANDLER_H
#define RTD_INPUT_HANDLER_H

#include "IO/BasePinHandler.h"

namespace IO {

class RTDInputHandler : public BasePinHandler {
public:
    RTDInputHandler() = default;
    virtual ~RTDInputHandler() = default;
    
    virtual bool initialize(const PinConfiguration& config) override;
    virtual void process() override;
    virtual bool setValue(float value) override;
    virtual PinState getState() const override { return state_; }
    virtual bool updateConfiguration(const PinConfiguration& config) override;
    
private:
    float filterAlpha_ = 0.05f;   // Low-pass filter for temperature (slower changes)
    float filteredTemp_ = 20.0f;  // Filtered temperature value
    bool usesFahrenheit_ = false; // Temperature unit
};

} // namespace IO

#endif // RTD_INPUT_HANDLER_H