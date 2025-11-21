#ifndef ANALOG_OUTPUT_HANDLER_H
#define ANALOG_OUTPUT_HANDLER_H

#include "IO/BasePinHandler.h"

namespace IO {

class AnalogOutputHandler : public BasePinHandler {
public:
    AnalogOutputHandler() = default;
    virtual ~AnalogOutputHandler() = default;
    
    virtual bool initialize(const PinConfiguration& config) override;
    virtual void process() override;
    virtual bool setValue(float value) override;
    virtual PinState getState() const override { return state_; }
    virtual bool updateConfiguration(const PinConfiguration& config) override;
};

} // namespace IO

#endif // ANALOG_OUTPUT_HANDLER_H
