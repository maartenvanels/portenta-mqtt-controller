#ifndef DIGITAL_OUTPUT_HANDLER_H
#define DIGITAL_OUTPUT_HANDLER_H

#include "IO/BasePinHandler.h"

namespace IO {

class DigitalOutputHandler : public BasePinHandler {
public:
    DigitalOutputHandler() = default;
    virtual ~DigitalOutputHandler() = default;
    
    virtual bool initialize(const PinConfiguration& config) override;
    virtual void process() override;
    virtual bool setValue(float value) override;
    virtual PinState getState() const override { return state_; }
    virtual bool updateConfiguration(const PinConfiguration& config) override;
    
private:
    bool currentState_ = false;
};

} // namespace IO

#endif // DIGITAL_OUTPUT_HANDLER_H