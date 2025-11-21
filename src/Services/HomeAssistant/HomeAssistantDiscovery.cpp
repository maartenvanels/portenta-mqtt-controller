#include "Services/HomeAssistant/HomeAssistantDiscovery.h"
#include <Arduino.h>

namespace HA
{

    bool HADiscovery::publishDiscovery(
        PubSubClient &mqttClient,
        IO::IoController &ioController,
        const char *deviceId,
        const char *deviceName)
    {
        if (!mqttClient.connected())
        {
            Serial.println("HA Discovery: MQTT not connected");
            return false;
        }

        Serial.println("\n=== Publishing Home Assistant Discovery ===");
        Serial.print("Device ID: ");
        Serial.println(deviceId);
        Serial.print("Device Name: ");
        Serial.println(deviceName);

        // Get all configured pins
        const auto pinNumbers = ioController.getAllPinNumbers();
        Serial.print("Total pins to publish: ");
        Serial.println(pinNumbers.size());

        int successCount = 0;
        int failCount = 0;

        // Publish discovery for each pin
        for (uint16_t key : pinNumbers)
        {
            IO::IPinHandler *handler = ioController.getPin(key);
            if (!handler)
            {
                continue;
            }

            const IO::PinConfiguration &config = handler->getConfiguration();

            // Skip pins without MQTT topic
            if (config.mqttTopic.empty())
            {
                Serial.print("Skipping pin without topic: ");
                Serial.println(config.name.c_str());
                continue;
            }

            bool success = false;
            const char *component = getHAComponent(config.type, config.mode);

            Serial.print("Publishing ");
            Serial.print(component);
            Serial.print(": ");
            Serial.print(config.name.c_str());
            Serial.print(" (");
            Serial.print(config.mqttTopic.c_str());
            Serial.print(")... ");

            // Determine which discovery message to publish based on component type
            if (strcmp(component, "switch") == 0)
            {
                success = publishSwitchDiscovery(mqttClient, deviceId, deviceName, config);
            }
            else if (strcmp(component, "binary_sensor") == 0)
            {
                success = publishBinarySensorDiscovery(mqttClient, deviceId, deviceName, config);
            }
            else if (strcmp(component, "sensor") == 0)
            {
                success = publishSensorDiscovery(mqttClient, deviceId, deviceName, config);
            }
            else if (strcmp(component, "number") == 0)
            {
                success = publishNumberDiscovery(mqttClient, deviceId, deviceName, config);
            }

            if (success)
            {
                Serial.println("OK");
                successCount++;
            }
            else
            {
                Serial.println("FAIL");
                failCount++;
            }

            // Small delay between publishes to avoid overwhelming the broker
            delay(50);
        }

        Serial.println("\n=== Discovery Summary ===");
        Serial.print("Success: ");
        Serial.println(successCount);
        Serial.print("Failed: ");
        Serial.println(failCount);

        return failCount == 0;
    }

    bool HADiscovery::removeDiscovery(
        PubSubClient &mqttClient,
        const char *deviceId,
        const char *pinTopic,
        IO::PinType pinType,
        IO::PinMode pinMode)
    {
        if (!mqttClient.connected())
        {
            return false;
        }

        const char *component = getHAComponent(pinType, pinMode);

        char topic[128];
        buildDiscoveryTopic(topic, sizeof(topic), component, deviceId, pinTopic);

        // Publish empty message to remove discovery
        return mqttClient.publish(topic, "", true);
    }

    bool HADiscovery::publishAvailability(
        PubSubClient &mqttClient,
        const char *deviceId,
        bool available)
    {
        if (!mqttClient.connected())
        {
            return false;
        }

        char topic[128];
        snprintf(topic, sizeof(topic), "portenta/%s/availability", deviceId);

        const char *payload = available ? "online" : "offline";
        return mqttClient.publish(topic, payload, true);
    }

    const char *HADiscovery::getHAComponent(IO::PinType pinType, IO::PinMode pinMode)
    {
        switch (pinType)
        {
        case IO::PinType::DIGITAL_OUTPUT:
            return "switch";

        case IO::PinType::DIGITAL_INPUT:
            return "binary_sensor";

        case IO::PinType::PROGRAMMABLE_DIO:
            // Depends on mode - input or output
            return (pinMode == IO::PinMode::OUTPUT) ? "switch" : "binary_sensor";

        case IO::PinType::ANALOG_INPUT_VOLTAGE:
        case IO::PinType::ANALOG_INPUT_CURRENT:
        case IO::PinType::RTD_INPUT:
        case IO::PinType::THERMOCOUPLE_INPUT:
        case IO::PinType::ENCODER_INPUT:
            return "sensor";

        case IO::PinType::ANALOG_OUTPUT_VOLTAGE:
        case IO::PinType::ANALOG_OUTPUT_CURRENT:
            return "number";

        default:
            return "sensor";
        }
    }

    void HADiscovery::buildDiscoveryTopic(
        char *buffer,
        size_t bufferSize,
        const char *component,
        const char *deviceId,
        const char *objectId)
    {
        snprintf(buffer, bufferSize, "%s/%s/%s/%s/config",
                 HA_DISCOVERY_PREFIX, component, deviceId, objectId);
    }

    void HADiscovery::buildDeviceInfo(
        ArduinoJson::JsonObject &deviceObj,
        const char *deviceId,
        const char *deviceName)
    {
        // Create identifiers array with single identifier
        ArduinoJson::JsonArray identifiers = deviceObj.createNestedArray("identifiers");
        identifiers.add(deviceId);

        deviceObj["name"] = deviceName;
        deviceObj["manufacturer"] = "Arduino";
        deviceObj["model"] = "Portenta H7 + Machine Control";
#ifndef BUILD_VERSION
#define BUILD_VERSION "1.0.0"
#endif
        deviceObj["sw_version"] = BUILD_VERSION;
    }

    bool HADiscovery::publishSwitchDiscovery(
        PubSubClient &mqttClient,
        const char *deviceId,
        const char *deviceName,
        const IO::PinConfiguration &config)
    {
        ArduinoJson::DynamicJsonDocument doc(DISCOVERY_JSON_SIZE);

        // Unique identifier for this entity
        String uniqueId = String(deviceId) + "_" + String(config.mqttTopic.c_str());
        doc["unique_id"] = uniqueId;

        // Human-readable name
        doc["name"] = config.name;

        // MQTT topics
        String stateTopic = "portenta/" + String(config.mqttTopic.c_str()) + "/state";
        String commandTopic = "portenta/" + String(config.mqttTopic.c_str()) + "/set";
        doc["state_topic"] = stateTopic;
        doc["command_topic"] = commandTopic;

        // Availability topic (shared for all entities)
        String availTopic = "portenta/" + String(deviceId) + "/availability";
        doc["availability_topic"] = availTopic;

        // Payload values
        doc["payload_on"] = "1";
        doc["payload_off"] = "0";
        doc["state_on"] = "1.000"; // Match the format from publishPinState
        doc["state_off"] = "0.000";

        // Optimistic mode (assume command succeeded)
        doc["optimistic"] = false;

        // Add device info
        ArduinoJson::JsonObject device = doc.createNestedObject("device");
        buildDeviceInfo(device, deviceId, deviceName);

        // Build discovery topic
        char topic[128];
        buildDiscoveryTopic(topic, sizeof(topic), "switch", deviceId, config.mqttTopic.c_str());

        // Serialize and publish
        String jsonString;
        serializeJson(doc, jsonString);

        return mqttClient.publish(topic, jsonString.c_str(), true);
    }

    bool HADiscovery::publishBinarySensorDiscovery(
        PubSubClient &mqttClient,
        const char *deviceId,
        const char *deviceName,
        const IO::PinConfiguration &config)
    {
        ArduinoJson::DynamicJsonDocument doc(DISCOVERY_JSON_SIZE);

        // Unique identifier
        String uniqueId = String(deviceId) + "_" + String(config.mqttTopic.c_str());
        doc["unique_id"] = uniqueId;

        // Name
        doc["name"] = config.name;

        // MQTT topic
        String stateTopic = "portenta/" + String(config.mqttTopic.c_str()) + "/state";
        doc["state_topic"] = stateTopic;

        // Availability
        String availTopic = "portenta/" + String(deviceId) + "/availability";
        doc["availability_topic"] = availTopic;

        // Payload values
        doc["payload_on"] = "1.000"; // Match the format from publishPinState
        doc["payload_off"] = "0.000";

        // Device class (optional - generic for now)
        // doc["device_class"] = "none";

        // Add device info
        ArduinoJson::JsonObject device = doc.createNestedObject("device");
        buildDeviceInfo(device, deviceId, deviceName);

        // Build discovery topic
        char topic[128];
        buildDiscoveryTopic(topic, sizeof(topic), "binary_sensor", deviceId, config.mqttTopic.c_str());

        // Serialize and publish
        String jsonString;
        serializeJson(doc, jsonString);

        return mqttClient.publish(topic, jsonString.c_str(), true);
    }

    bool HADiscovery::publishSensorDiscovery(
        PubSubClient &mqttClient,
        const char *deviceId,
        const char *deviceName,
        const IO::PinConfiguration &config)
    {
        ArduinoJson::DynamicJsonDocument doc(DISCOVERY_JSON_SIZE);

        // Unique identifier
        String uniqueId = String(deviceId) + "_" + String(config.mqttTopic.c_str());
        doc["unique_id"] = uniqueId;

        // Name
        doc["name"] = config.name;

        // MQTT topic
        String stateTopic = "portenta/" + String(config.mqttTopic.c_str()) + "/state";
        doc["state_topic"] = stateTopic;

        // Availability
        String availTopic = "portenta/" + String(deviceId) + "/availability";
        doc["availability_topic"] = availTopic;

        // Unit of measurement
        if (!config.unit.empty())
        {
            doc["unit_of_measurement"] = config.unit.c_str();
        }
        else
        {
            const char *unit = getUnitOfMeasurement(config.type);
            if (unit)
            {
                doc["unit_of_measurement"] = unit;
            }

            // Device class (only use default if using default unit)
            const char *deviceClass = getDeviceClass(config.type);
            if (deviceClass)
            {
                doc["device_class"] = deviceClass;
            }
        }

        // State class (for statistics in HA)
        doc["state_class"] = "measurement";

        // Display precision (number of decimal places)
        doc["suggested_display_precision"] = config.precision;

        // Value template (optional - for formatting)
        // doc["value_template"] = "{{ value | float }}";

        // Add device info
        ArduinoJson::JsonObject device = doc.createNestedObject("device");
        buildDeviceInfo(device, deviceId, deviceName);

        // Build discovery topic
        char topic[128];
        buildDiscoveryTopic(topic, sizeof(topic), "sensor", deviceId, config.mqttTopic.c_str());

        // Serialize and publish
        String jsonString;
        serializeJson(doc, jsonString);

        return mqttClient.publish(topic, jsonString.c_str(), true);
    }

    bool HADiscovery::publishNumberDiscovery(
        PubSubClient &mqttClient,
        const char *deviceId,
        const char *deviceName,
        const IO::PinConfiguration &config)
    {
        ArduinoJson::DynamicJsonDocument doc(DISCOVERY_JSON_SIZE);

        // Unique identifier
        String uniqueId = String(deviceId) + "_" + String(config.mqttTopic.c_str());
        doc["unique_id"] = uniqueId;

        // Name
        doc["name"] = config.name;

        // MQTT topics
        String stateTopic = "portenta/" + String(config.mqttTopic.c_str()) + "/state";
        String commandTopic = "portenta/" + String(config.mqttTopic.c_str()) + "/set";
        doc["state_topic"] = stateTopic;
        doc["command_topic"] = commandTopic;

        // Availability
        String availTopic = "portenta/" + String(deviceId) + "/availability";
        doc["availability_topic"] = availTopic;

        // Min/max values
        if (config.minValue != 0.0f || config.maxValue != 0.0f)
        {
            doc["min"] = config.minValue;
            doc["max"] = config.maxValue;
        }
        else
        {
            // Default ranges for analog outputs
            if (config.type == IO::PinType::ANALOG_OUTPUT_VOLTAGE)
            {
                doc["min"] = 0.0f;
                doc["max"] = 10.0f;
            }
            else if (config.type == IO::PinType::ANALOG_OUTPUT_CURRENT)
            {
                doc["min"] = 4.0f;
                doc["max"] = 20.0f;
            }
        }

        // Step size
        doc["step"] = 0.001f; // Allow millivolt precision

        // Display precision
        doc["suggested_display_precision"] = config.precision;

        // Unit of measurement
        if (!config.unit.empty())
        {
            doc["unit_of_measurement"] = config.unit.c_str();
        }
        else
        {
            const char *unit = getUnitOfMeasurement(config.type);
            if (unit)
            {
                doc["unit_of_measurement"] = unit;
            }
        }

        // Mode
        doc["mode"] = "slider"; // or "box" for input field

        // Add device info
        ArduinoJson::JsonObject device = doc.createNestedObject("device");
        buildDeviceInfo(device, deviceId, deviceName);

        // Build discovery topic
        char topic[128];
        buildDiscoveryTopic(topic, sizeof(topic), "number", deviceId, config.mqttTopic.c_str());

        // Serialize and publish
        String jsonString;
        serializeJson(doc, jsonString);

        return mqttClient.publish(topic, jsonString.c_str(), true);
    }

    const char *HADiscovery::getUnitOfMeasurement(IO::PinType pinType)
    {
        switch (pinType)
        {
        case IO::PinType::ANALOG_INPUT_VOLTAGE:
        case IO::PinType::ANALOG_OUTPUT_VOLTAGE:
            return "V";

        case IO::PinType::ANALOG_INPUT_CURRENT:
        case IO::PinType::ANALOG_OUTPUT_CURRENT:
            return "mA";

        case IO::PinType::RTD_INPUT:
        case IO::PinType::THERMOCOUPLE_INPUT:
            return "°C";

        case IO::PinType::ENCODER_INPUT:
            return "pulses";

        default:
            return nullptr;
        }
    }

    const char *HADiscovery::getDeviceClass(IO::PinType pinType)
    {
        switch (pinType)
        {
        case IO::PinType::ANALOG_INPUT_VOLTAGE:
        case IO::PinType::ANALOG_OUTPUT_VOLTAGE:
            return "voltage";

        case IO::PinType::ANALOG_INPUT_CURRENT:
        case IO::PinType::ANALOG_OUTPUT_CURRENT:
            return "current";

        case IO::PinType::RTD_INPUT:
        case IO::PinType::THERMOCOUPLE_INPUT:
            return "temperature";

        default:
            return nullptr;
        }
    }

} // namespace HA
