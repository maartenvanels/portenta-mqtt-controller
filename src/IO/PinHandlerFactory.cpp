#include "IO/PinHandlerFactory.h"
#include "IO/DigitalOutputHandler.h"
#include "IO/DigitalInputHandler.h"
#include "IO/AnalogInputHandler.h"
#include "IO/RTDInputHandler.h"
#include "IO/ProgrammableDIOHandler.h"
#include "IO/AnalogOutputHandler.h"

namespace IO {

std::unique_ptr<IPinHandler> PinHandlerFactory::create(const PinConfiguration& config) {
    std::unique_ptr<IPinHandler> handler;
    
    switch (config.type) {
        case PinType::DIGITAL_OUTPUT:
            handler = std::make_unique<DigitalOutputHandler>();
            break;

        case PinType::DIGITAL_INPUT:
            handler = std::make_unique<DigitalInputHandler>();
            break;

        case PinType::ANALOG_INPUT_VOLTAGE:
        case PinType::ANALOG_INPUT_CURRENT:
            handler = std::make_unique<AnalogInputHandler>();
            break;

        case PinType::RTD_INPUT:
            handler = std::make_unique<RTDInputHandler>();
            break;

        case PinType::PROGRAMMABLE_DIO:
            handler = std::make_unique<ProgrammableDIOHandler>();
            break;

        case PinType::ANALOG_OUTPUT_VOLTAGE:
        case PinType::ANALOG_OUTPUT_CURRENT:
            handler = std::make_unique<AnalogOutputHandler>();
            break;

        // TODO: Add other pin types as handlers are implemented
        case PinType::THERMOCOUPLE_INPUT:
        case PinType::ENCODER_INPUT:
        default:
            return nullptr;
    }
    
    if (handler && handler->initialize(config)) {
        return handler;
    }
    
    return nullptr;
}

std::unique_ptr<IPinHandler> PinHandlerFactory::create(PinType type, uint8_t pin) {
    PinConfiguration config;
    config.pinNumber = pin;
    config.type = type;
    config.name = "Pin_" + std::to_string(pin);
    config.mqttTopic = "pins/" + std::to_string(pin);
    
    // Set default configurations based on type
    switch (type) {
        case PinType::DIGITAL_OUTPUT:
            config.mode = PinMode::OUTPUT;
            config.debounceMs = 0;
            config.sampleRateMs = 100;
            break;
            
        case PinType::ANALOG_INPUT_VOLTAGE:
            config.mode = PinMode::INPUT;
            config.minValue = 0.0f;
            config.maxValue = 10.0f;
            config.sampleRateMs = 100;
            break;
            
        case PinType::ANALOG_INPUT_CURRENT:
            config.mode = PinMode::INPUT;
            config.minValue = 4.0f;
            config.maxValue = 20.0f;
            config.sampleRateMs = 100;
            break;
            
        case PinType::RTD_INPUT:
            config.mode = PinMode::INPUT;
            config.minValue = -200.0f;
            config.maxValue = 850.0f;
            config.sampleRateMs = 1000;  // Sample every second for temperature
            break;
            
        case PinType::ANALOG_OUTPUT_VOLTAGE:
            config.mode = PinMode::OUTPUT;
            config.minValue = 0.0f;
            config.maxValue = 10.0f;
            break;
            
        case PinType::ANALOG_OUTPUT_CURRENT:
            config.mode = PinMode::OUTPUT;
            config.minValue = 4.0f;
            config.maxValue = 20.0f;
            break;
            
        default:
            break;
    }
    
    return create(config);
}

} // namespace IO
