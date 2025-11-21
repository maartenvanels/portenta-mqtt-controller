#include "IO/AnalogInputHandler.h"

namespace IO
{

    bool AnalogInputHandler::initialize(const PinConfiguration &config)
    {
        if (!BasePinHandler::initialize(config))
        {
            return false;
        }

        // Validate pin number for analog inputs (0-2)
        if (config_.pinNumber > 2)
        {
            setError("Invalid analog input pin number. Must be 0-2.");
            return false;
        }

        // Determine if current or voltage mode
        isCurrentMode_ = (config_.type == PinType::ANALOG_INPUT_CURRENT);

        // Initialize filtered value
        filteredValue_ = 0.0f;

        return true;
    }

    void AnalogInputHandler::process()
    {
        if (!shouldSample())
        {
            return;
        }

        lastSampleTime_ = millis();

        // Read analog value from hardware (returns 0-10V)
        float rawVoltage = hal_.readAnalogInput(config_.pinNumber);

        if (rawVoltage < 0.0f)
        {
            setError("Failed to read analog input");
            return;
        }

        float processedValue = rawVoltage;

        // Convert to current if in current mode
        if (isCurrentMode_)
        {
            // Convert voltage to current (4-20mA)
            // Assuming a 250 ohm shunt resistor: 4mA = 1V, 20mA = 5V
            // Linear conversion: current = (voltage - 1.0) * 4.0 + 4.0
            if (rawVoltage < 1.0f)
            {
                processedValue = 4.0f; // Minimum 4mA
            }
            else if (rawVoltage > 5.0f)
            {
                processedValue = 20.0f; // Maximum 20mA
            }
            else
            {
                processedValue = (rawVoltage - 1.0f) * 4.0f + 4.0f;
            }
        }

        // Apply lookup table or scale/offset
        if (!config_.lookupTable.empty())
        {
            // Find segment in lookup table
            const auto &table = config_.lookupTable;
            if (processedValue <= table.front().first)
            {
                processedValue = table.front().second;
            }
            else if (processedValue >= table.back().first)
            {
                processedValue = table.back().second;
            }
            else
            {
                for (size_t i = 0; i < table.size() - 1; ++i)
                {
                    if (processedValue >= table[i].first && processedValue <= table[i + 1].first)
                    {
                        float t = (processedValue - table[i].first) / (table[i + 1].first - table[i].first);
                        processedValue = table[i].second + t * (table[i + 1].second - table[i].second);
                        break;
                    }
                }
            }
        }
        else
        {
            // Apply scale and offset
            processedValue = (processedValue * config_.scaleFactor) + config_.offset;
        }

        // Apply low-pass filter for noise reduction
        filteredValue_ = (filterAlpha_ * processedValue) + ((1.0f - filterAlpha_) * filteredValue_);

        // Update state with filtered value
        updateState(filteredValue_);
        clearError();
    }

    bool AnalogInputHandler::setValue(float value)
    {
        // Analog inputs are read-only
        setError("Cannot set value on analog input");
        return false;
    }

    bool AnalogInputHandler::updateConfiguration(const PinConfiguration &config)
    {
        if (config.type != PinType::ANALOG_INPUT_VOLTAGE &&
            config.type != PinType::ANALOG_INPUT_CURRENT)
        {
            return false;
        }

        config_ = config;
        isCurrentMode_ = (config_.type == PinType::ANALOG_INPUT_CURRENT);

        // Update filter coefficient based on sample rate
        // Higher sample rate = lower filter coefficient for smoother output
        filterAlpha_ = 1.0f / (1.0f + (1000.0f / config_.sampleRateMs));

        return true;
    }

} // namespace IO
