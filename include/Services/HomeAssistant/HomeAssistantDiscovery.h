#ifndef HA_DISCOVERY_H
#define HA_DISCOVERY_H

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "IO/IPinHandler.h"
#include "IO/IoController.h"

namespace HA {

/**
 * Home Assistant MQTT Discovery
 *
 * Publishes device and entity configurations to Home Assistant's
 * discovery topics, enabling automatic integration.
 *
 * Discovery Topic Format:
 * homeassistant/<component>/<node_id>/<object_id>/config
 *
 * Supported Components:
 * - switch: Digital outputs, programmable outputs
 * - binary_sensor: Digital inputs, programmable inputs
 * - sensor: Analog inputs, RTD, Thermocouple, Encoder
 * - number: Analog outputs
 */
class HADiscovery {
public:
    /**
     * Publish discovery messages for all configured pins
     *
     * @param mqttClient MQTT client to publish through
     * @param ioController I/O controller with pin configurations
     * @param deviceId Unique device identifier (e.g., "portenta_h7")
     * @param deviceName Human-readable device name (e.g., "Portenta Machine Control")
     * @return true if all discovery messages published successfully
     */
    static bool publishDiscovery(
        PubSubClient& mqttClient,
        IO::IoController& ioController,
        const char* deviceId,
        const char* deviceName
    );

    /**
     * Remove discovery for a specific pin (useful when removing pins)
     *
     * @param mqttClient MQTT client to publish through
     * @param deviceId Device identifier
     * @param pinTopic Pin's MQTT topic
     * @param pinType Pin type (determines HA component)
     * @param pinMode Pin mode (determines if input or output for DIO)
     */
    static bool removeDiscovery(
        PubSubClient& mqttClient,
        const char* deviceId,
        const char* pinTopic,
        IO::PinType pinType,
        IO::PinMode pinMode
    );

    /**
     * Publish device availability status
     *
     * @param mqttClient MQTT client to publish through
     * @param deviceId Device identifier
     * @param available true for "online", false for "offline"
     */
    static bool publishAvailability(
        PubSubClient& mqttClient,
        const char* deviceId,
        bool available
    );

private:
    // Maximum size for discovery JSON documents
    static constexpr size_t DISCOVERY_JSON_SIZE = 1024;

    // Home Assistant discovery prefix
    static constexpr const char* HA_DISCOVERY_PREFIX = "homeassistant";

    /**
     * Get Home Assistant component type for a pin
     *
     * @param pinType Pin type
     * @param pinMode Pin mode (for DIO pins)
     * @return HA component name ("switch", "binary_sensor", "sensor", "number")
     */
    static const char* getHAComponent(IO::PinType pinType, IO::PinMode pinMode);

    /**
     * Build discovery topic for a pin
     *
     * @param buffer Output buffer
     * @param bufferSize Buffer size
     * @param component HA component type
     * @param deviceId Device identifier
     * @param objectId Object identifier (usually pin topic)
     */
    static void buildDiscoveryTopic(
        char* buffer,
        size_t bufferSize,
        const char* component,
        const char* deviceId,
        const char* objectId
    );

    /**
     * Build device info JSON object (shared across all entities)
     *
     * @param deviceObj JSON object to populate
     * @param deviceId Device identifier
     * @param deviceName Device name
     */
    static void buildDeviceInfo(
        ArduinoJson::JsonObject& deviceObj,
        const char* deviceId,
        const char* deviceName
    );

    /**
     * Create discovery message for a switch (output)
     */
    static bool publishSwitchDiscovery(
        PubSubClient& mqttClient,
        const char* deviceId,
        const char* deviceName,
        const IO::PinConfiguration& config
    );

    /**
     * Create discovery message for a binary sensor (input)
     */
    static bool publishBinarySensorDiscovery(
        PubSubClient& mqttClient,
        const char* deviceId,
        const char* deviceName,
        const IO::PinConfiguration& config
    );

    /**
     * Create discovery message for a sensor (analog input, temperature, etc.)
     */
    static bool publishSensorDiscovery(
        PubSubClient& mqttClient,
        const char* deviceId,
        const char* deviceName,
        const IO::PinConfiguration& config
    );

    /**
     * Create discovery message for a number (analog output)
     */
    static bool publishNumberDiscovery(
        PubSubClient& mqttClient,
        const char* deviceId,
        const char* deviceName,
        const IO::PinConfiguration& config
    );

    /**
     * Get unit of measurement for sensor types
     */
    static const char* getUnitOfMeasurement(IO::PinType pinType);

    /**
     * Get device class for sensor types (temperature, voltage, current, etc.)
     */
    static const char* getDeviceClass(IO::PinType pinType);
};

} // namespace HA

#endif // HA_DISCOVERY_H
