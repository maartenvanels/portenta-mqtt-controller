#ifndef MACHINE_CONTROL_HAL_H
#define MACHINE_CONTROL_HAL_H

#include <Arduino_PortentaMachineControl.h>
#undef abs
#include <memory>
#include <functional>
#include <array>

namespace HAL {

class MachineControlHAL {
public:
    static MachineControlHAL& getInstance() {
        static MachineControlHAL instance;
        return instance;
    }

    bool initialize();
    
    // Digital I/O Access
    bool setDigitalOutput(uint8_t channel, bool state);
    bool readDigitalInput(uint8_t channel);
    
    // Programmable Digital I/O
    bool configureProgrammableIO(uint8_t channel, bool asOutput);
    bool setProgrammableOutput(uint8_t channel, bool state);
    bool readProgrammableInput(uint8_t channel);
    
    // Analog I/O Access
    float readAnalogInput(uint8_t channel);
    bool setAnalogOutput(uint8_t channel, float voltage);
    
    // Temperature Probes
    float readRTDTemperature(uint8_t channel);
    float readThermocoupleTemperature(uint8_t channel);
    
    // Communication Interfaces
    bool initializeRS485(uint32_t baudRate);
    bool initializeCAN(uint32_t baudRate);
    
    // RTC Functions
    bool setRTCTime(uint32_t unixTime);
    uint32_t getRTCTime();
    
    // Encoder Functions
    int32_t readEncoder(uint8_t channel);
    void resetEncoder(uint8_t channel);

private:
    MachineControlHAL() = default;
    ~MachineControlHAL() = default;
    
    MachineControlHAL(const MachineControlHAL&) = delete;
    MachineControlHAL& operator=(const MachineControlHAL&) = delete;
    
    bool initialized = false;
    std::array<bool, 8> digitalOutputState_{};
    SensorType analogInputMode_ = SensorType::V_0_10;
};

// Pin mapping constants for Portenta Machine Control
namespace Pins {
    // Digital Inputs (24V tolerant)
    constexpr uint8_t DIGITAL_IN_0 = 0;
    constexpr uint8_t DIGITAL_IN_1 = 1;
    constexpr uint8_t DIGITAL_IN_2 = 2;
    constexpr uint8_t DIGITAL_IN_3 = 3;
    constexpr uint8_t DIGITAL_IN_4 = 4;
    constexpr uint8_t DIGITAL_IN_5 = 5;
    constexpr uint8_t DIGITAL_IN_6 = 6;
    constexpr uint8_t DIGITAL_IN_7 = 7;

    // Digital Outputs (24V tolerant)
    constexpr uint8_t DIGITAL_OUT_0 = 0;
    constexpr uint8_t DIGITAL_OUT_1 = 1;
    constexpr uint8_t DIGITAL_OUT_2 = 2;
    constexpr uint8_t DIGITAL_OUT_3 = 3;
    constexpr uint8_t DIGITAL_OUT_4 = 4;
    constexpr uint8_t DIGITAL_OUT_5 = 5;
    constexpr uint8_t DIGITAL_OUT_6 = 6;
    constexpr uint8_t DIGITAL_OUT_7 = 7;

    // Programmable Digital I/O
    constexpr uint8_t PROGRAMMABLE_DIO_0 = 0;
    constexpr uint8_t PROGRAMMABLE_DIO_1 = 1;
    constexpr uint8_t PROGRAMMABLE_DIO_2 = 2;
    constexpr uint8_t PROGRAMMABLE_DIO_3 = 3;
    constexpr uint8_t PROGRAMMABLE_DIO_4 = 4;
    constexpr uint8_t PROGRAMMABLE_DIO_5 = 5;
    constexpr uint8_t PROGRAMMABLE_DIO_6 = 6;
    constexpr uint8_t PROGRAMMABLE_DIO_7 = 7;
    constexpr uint8_t PROGRAMMABLE_DIO_8 = 8;
    constexpr uint8_t PROGRAMMABLE_DIO_9 = 9;
    constexpr uint8_t PROGRAMMABLE_DIO_10 = 10;
    constexpr uint8_t PROGRAMMABLE_DIO_11 = 11;
    
    // Analog Inputs (0-10V / 4-20mA)
    constexpr uint8_t ANALOG_IN_0 = 0;
    constexpr uint8_t ANALOG_IN_1 = 1;
    constexpr uint8_t ANALOG_IN_2 = 2;
    
    // Analog Outputs (0-10V / 4-20mA)
    constexpr uint8_t ANALOG_OUT_0 = 0;
    constexpr uint8_t ANALOG_OUT_1 = 1;
    constexpr uint8_t ANALOG_OUT_2 = 2;
    constexpr uint8_t ANALOG_OUT_3 = 3;
    
    // Temperature Probes
    constexpr uint8_t RTD_CH0 = 0;
    constexpr uint8_t RTD_CH1 = 1;
    constexpr uint8_t RTD_CH2 = 2;
    
    constexpr uint8_t TC_CH0 = 0;
    constexpr uint8_t TC_CH1 = 1;
    constexpr uint8_t TC_CH2 = 2;
}

} // namespace HAL

#endif // MACHINE_CONTROL_HAL_H
