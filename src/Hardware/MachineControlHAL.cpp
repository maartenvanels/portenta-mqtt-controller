#include "Hardware/MachineControlHAL.h"
#include <Arduino.h>
#undef abs

namespace {
// Digital input pin constants - these map channels 0-7 to the actual GPIO expander pins
constexpr int kDigitalInputPins[] = {
    DIN_READ_CH_PIN_00, DIN_READ_CH_PIN_01, DIN_READ_CH_PIN_02, DIN_READ_CH_PIN_03,
    DIN_READ_CH_PIN_04, DIN_READ_CH_PIN_05, DIN_READ_CH_PIN_06, DIN_READ_CH_PIN_07
};

// Programmable DIO pin constants - these map channels 0-11 to the actual GPIO expander pins
constexpr int kProgrammableWritePins[] = {
    IO_WRITE_CH_PIN_00, IO_WRITE_CH_PIN_01, IO_WRITE_CH_PIN_02, IO_WRITE_CH_PIN_03,
    IO_WRITE_CH_PIN_04, IO_WRITE_CH_PIN_05, IO_WRITE_CH_PIN_06, IO_WRITE_CH_PIN_07,
    IO_WRITE_CH_PIN_08, IO_WRITE_CH_PIN_09, IO_WRITE_CH_PIN_10, IO_WRITE_CH_PIN_11
};

constexpr int kProgrammableReadPins[] = {
    IO_READ_CH_PIN_00, IO_READ_CH_PIN_01, IO_READ_CH_PIN_02, IO_READ_CH_PIN_03,
    IO_READ_CH_PIN_04, IO_READ_CH_PIN_05, IO_READ_CH_PIN_06, IO_READ_CH_PIN_07,
    IO_READ_CH_PIN_08, IO_READ_CH_PIN_09, IO_READ_CH_PIN_10, IO_READ_CH_PIN_11
};

constexpr size_t kDigitalInputCount = sizeof(kDigitalInputPins) / sizeof(kDigitalInputPins[0]);
constexpr size_t kProgrammableChannelCount = sizeof(kProgrammableWritePins) / sizeof(kProgrammableWritePins[0]);
constexpr size_t kProgrammableReadChannelCount = sizeof(kProgrammableReadPins) / sizeof(kProgrammableReadPins[0]);

constexpr float kAnalogReferenceVoltage = 3.0f;
constexpr float kAnalogDividerRatio = 0.28057f;
constexpr float kAnalogScale = kAnalogReferenceVoltage / kAnalogDividerRatio;
constexpr float kMaxAnalogCount = 65535.0f;
constexpr float kMaxAnalogOutputVoltage = 10.5f;
constexpr float kRtdReferenceResistor = 400.0f;
constexpr float kRtdNominalResistance = 100.0f;
constexpr uint32_t kDefaultRs485PreambleUs = 0;
constexpr uint32_t kDefaultRs485PostambleUs = 0;

CanBitRate toCanBitRate(uint32_t baudRate) {
    switch (baudRate) {
        case 1000000: return CanBitRate::BR_1000k;
        case 500000:  return CanBitRate::BR_500k;
        case 250000:  return CanBitRate::BR_250k;
        case 125000:  return CanBitRate::BR_125k;
        default:      return CanBitRate::BR_500k;
    }
}
} // namespace

namespace HAL {

bool MachineControlHAL::initialize() {
    if (initialized) {
        return true;
    }

    Wire.begin();
    Wire1.begin();  // For RTC - works with WiFi, conflicts with Ethernet

    if (!MachineControl_DigitalOutputs.begin(true)) {
        return false;
    }
    MachineControl_DigitalOutputs.writeAll(0);
    digitalOutputState_.fill(false);

    if (!MachineControl_DigitalProgrammables.begin(true)) {
        return false;
    }

    if (!MachineControl_DigitalInputs.begin()) {
        return false;
    }

    analogInputMode_ = SensorType::V_0_10;
    if (!MachineControl_AnalogIn.begin(analogInputMode_)) {
        return false;
    }

    if (!MachineControl_AnalogOut.begin()) {
        return false;
    }

    if (!MachineControl_RTDTempProbe.begin(THREE_WIRE)) {
        return false;
    }

    if (!MachineControl_TCTempProbe.begin()) {
        return false;
    }

    // RTC disabled - requires Wire1 which conflicts with Ethernet
    // if (!MachineControl_RTCController.begin()) {
    //     return false;
    // }

    initialized = true;
    return true;
}

bool MachineControlHAL::setDigitalOutput(uint8_t channel, bool state) {
    if (!initialized || channel >= digitalOutputState_.size()) {
        return false;
    }

    MachineControl_DigitalOutputs.write(channel, state ? HIGH : LOW);
    digitalOutputState_[channel] = state;
    return true;
}

bool MachineControlHAL::readDigitalInput(uint8_t channel) {
    if (!initialized || channel >= kDigitalInputCount) {
        return false;
    }

    // Use the pin constant from the mapping array
    return MachineControl_DigitalInputs.read(kDigitalInputPins[channel]) != 0;
}

bool MachineControlHAL::configureProgrammableIO(uint8_t channel, bool asOutput) {
    if (!initialized || channel >= kProgrammableChannelCount) {
        return false;
    }

    // Programmable DIO pins don't need pinMode configuration
    // The library uses bidirectional pins - WRITE and READ pins are always active
    // Just initialize the output to LOW if it's an output
    if (asOutput) {
        return MachineControl_DigitalProgrammables.set(
            kProgrammableWritePins[channel],
            SWITCH_OFF
        );
    }

    // For inputs, no configuration needed
    return true;
}

bool MachineControlHAL::setProgrammableOutput(uint8_t channel, bool state) {
    if (!initialized || channel >= kProgrammableChannelCount) {
        return false;
    }

    return MachineControl_DigitalProgrammables.set(
        kProgrammableWritePins[channel],
        state ? SWITCH_ON : SWITCH_OFF
    );
}

bool MachineControlHAL::readProgrammableInput(uint8_t channel) {
    if (!initialized || channel >= kProgrammableReadChannelCount) {
        return false;
    }

    return MachineControl_DigitalProgrammables.read(kProgrammableReadPins[channel]) != 0;
}

float MachineControlHAL::readAnalogInput(uint8_t channel) {
    if (!initialized || channel > 2) {
        return -1.0f;
    }

    const uint16_t raw = MachineControl_AnalogIn.read(channel);
    return (static_cast<float>(raw) / kMaxAnalogCount) * kAnalogScale;
}

bool MachineControlHAL::setAnalogOutput(uint8_t channel, float voltage) {
    if (!initialized || channel > 3) {
        return false;
    }

    float clamped = voltage;
    if (clamped < 0.0f) {
        clamped = 0.0f;
    } else if (clamped > kMaxAnalogOutputVoltage) {
        clamped = kMaxAnalogOutputVoltage;
    }
    MachineControl_AnalogOut.write(channel, clamped);
    return true;
}

float MachineControlHAL::readRTDTemperature(uint8_t channel) {
    if (!initialized || channel > 2) {
        return NAN;
    }

    MachineControl_RTDTempProbe.selectChannel(channel);
    return MachineControl_RTDTempProbe.readTemperature(kRtdNominalResistance, kRtdReferenceResistor);
}

float MachineControlHAL::readThermocoupleTemperature(uint8_t channel) {
    if (!initialized || channel > 2) {
        return NAN;
    }

    MachineControl_TCTempProbe.selectChannel(channel);
    return MachineControl_TCTempProbe.readTemperature();
}

bool MachineControlHAL::initializeRS485(uint32_t baudRate) {
    if (!initialized) {
        return false;
    }

    MachineControl_RS485Comm.begin(baudRate, kDefaultRs485PreambleUs, kDefaultRs485PostambleUs);
    MachineControl_RS485Comm.receive();
    return true;
}

bool MachineControlHAL::initializeCAN(uint32_t baudRate) {
    if (!initialized) {
        return false;
    }

    return MachineControl_CANComm.begin(toCanBitRate(baudRate));
}

bool MachineControlHAL::setRTCTime(uint32_t unixTime) {
    if (!initialized) {
        return false;
    }

    MachineControl_RTCController.setEpoch(unixTime);
    return true;
}

uint32_t MachineControlHAL::getRTCTime() {
    if (!initialized) {
        return 0;
    }

    return static_cast<uint32_t>(MachineControl_RTCController.getEpoch());
}

int32_t MachineControlHAL::readEncoder(uint8_t channel) {
    if (!initialized || channel > 1) {
        return 0;
    }

    return MachineControl_Encoders.getPulses(channel);
}

void MachineControlHAL::resetEncoder(uint8_t channel) {
    if (!initialized || channel > 1) {
        return;
    }

    MachineControl_Encoders.reset(channel);
}

} // namespace HAL
