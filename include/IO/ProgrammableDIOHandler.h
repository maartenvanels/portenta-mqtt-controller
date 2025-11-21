#pragma once

#include "IO/BasePinHandler.h"

namespace IO {

class ProgrammableDIOHandler : public BasePinHandler {
public:
    ProgrammableDIOHandler() = default;
    ~ProgrammableDIOHandler() override = default;

    bool initialize(const PinConfiguration& config) override;
    bool setValue(float value) override;
    PinState getState() const override { return state_; }
    void process() override;
    bool updateConfiguration(const PinConfiguration& config) override;

private:
    bool isOutput_;
    bool lastInputState_;
};

} // namespace IO
