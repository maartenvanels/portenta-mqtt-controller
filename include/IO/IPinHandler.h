#ifndef IPIN_HANDLER_H
#define IPIN_HANDLER_H

#include <Arduino.h>
#undef abs
#include <ArduinoJson.h>
#include <functional>
#include <string>
#include <vector>
#include <utility>

namespace IO
{

    enum class PinType
    {
        DIGITAL_OUTPUT,        // 24V digital outputs
        DIGITAL_INPUT,         // 24V digital inputs
        PROGRAMMABLE_DIO,      // Programmable digital I/O (mode determines input/output)
        ANALOG_INPUT_VOLTAGE,  // 0-10V analog input
        ANALOG_INPUT_CURRENT,  // 4-20mA analog input
        ANALOG_OUTPUT_VOLTAGE, // 0-10V analog output
        ANALOG_OUTPUT_CURRENT, // 4-20mA analog output
        RTD_INPUT,             // RTD temperature probe
        THERMOCOUPLE_INPUT,    // Thermocouple temperature probe
        ENCODER_INPUT          // Encoder input
    };

    enum class PinMode
    {
        INPUT,
        OUTPUT,
        INPUT_PULLUP,
        INPUT_PULLDOWN
    };

    struct PinConfiguration
    {
        uint8_t pinNumber;
        PinType type;
        PinMode mode;
        std::string name;
        std::string mqttTopic;
        float scaleFactor = 1.0f;
        float offset = 0.0f;
        uint32_t debounceMs = 50;
        uint32_t sampleRateMs = 100;
        float minValue = 0.0f;
        float maxValue = 0.0f;
        bool invertLogic = false;
        uint8_t precision = 2;
        std::string unit;
        std::vector<std::pair<float, float>> lookupTable;
    };

    struct PinState
    {
        float value;
        float rawValue;
        bool isValid;
        uint32_t lastUpdateTime;
        uint32_t changeCount;
    };

    using StateChangeCallback = std::function<void(uint8_t pin, const PinState &state)>;

    class IPinHandler
    {
    public:
        virtual ~IPinHandler() = default;

        // Lifecycle methods
        virtual bool initialize(const PinConfiguration &config) = 0;
        virtual void shutdown() = 0;

        // Core functionality
        virtual void process() = 0;
        virtual bool setValue(float value) = 0;
        virtual PinState getState() const = 0;

        // Configuration
        virtual PinConfiguration getConfiguration() const = 0;
        virtual bool updateConfiguration(const PinConfiguration &config) = 0;

        // State change notification
        virtual void setStateChangeCallback(StateChangeCallback callback) = 0;

        // Serialization
        virtual ArduinoJson::DynamicJsonDocument toJson() const = 0;
        virtual bool fromJson(const ArduinoJson::JsonVariantConst &doc) = 0;

        // Diagnostics
        virtual bool isHealthy() const = 0;
        virtual std::string getErrorMessage() const = 0;
    };

} // namespace IO

#endif // IPIN_HANDLER_H
