#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <functional>
#include <string>

class ConnectionHandler;

// Forward declarations
namespace IO {
    class IoController;
    struct PinState;
}

/**
 * @brief Manages MQTT connection and communication
 *
 * Single Responsibility: MQTT broker communication
 * - Handles MQTT connection lifecycle
 * - Manages subscriptions
 * - Routes incoming messages
 * - Publishes pin states and system status
 * - Handles reconnection logic
 */
class MQTTManager {
public:
    /**
     * @brief Message handler callback type
     * @param topic MQTT topic
     * @param payload Message payload
     * @param length Payload length
     */
    using MessageCallback = std::function<void(const char* topic, const byte* payload, unsigned int length)>;

    /**
     * @brief Get singleton instance
     */
    static MQTTManager& getInstance();

    /**
     * @brief Initialize MQTT manager with client
     * @param client Network client (WiFi or Ethernet)
     */
    void initialize(PubSubClient& client);

    /**
     * @brief Connect to MQTT broker
     * @param connectionHandler Arduino ConnectionHandler providing the active Client
     * @param ioController IoController instance for Home Assistant discovery
     * @return true if connected, false otherwise
     */
    bool connect(ConnectionHandler& connectionHandler, IO::IoController& ioController);

    /**
     * @brief Check if connected to MQTT broker
     * @return true if connected, false otherwise
     */
    bool isConnected();

    /**
     * @brief Maintain MQTT connection (call in loop)
     * Processes incoming messages and handles reconnection
     */
    void loop();

    /**
     * @brief Publish pin state to MQTT
     * @param pin Pin number
     * @param state Pin state
     * @param topic MQTT topic
     */
    void publishPinState(uint8_t pin, const IO::PinState& state, const std::string& topic);

    /**
     * @brief Publish all pin states
     * @param ioController IoController instance
     */
    void publishAllPinStates(IO::IoController& ioController);

    /**
     * @brief Publish system status
     */
    void publishSystemStatus();

    /**
     * @brief Set message handler callback
     */
    void setMessageCallback(MessageCallback callback);

    /**
     * @brief Get MQTT client state
     * @return PubSubClient state code
     */
    int getState();

private:
    MQTTManager();  // Singleton pattern
    MQTTManager(const MQTTManager&) = delete;
    MQTTManager& operator=(const MQTTManager&) = delete;

    // Static callback wrapper for PubSubClient
    static void messageCallbackWrapper(char* topic, byte* payload, unsigned int length);

    PubSubClient* mqttClient_;
    MessageCallback messageCallback_;
    uint32_t lastConnectionAttempt_;
    bool lastConnectedState_;

    static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;
    static constexpr const char* HA_DEVICE_ID = "portenta_h7";
    static constexpr const char* HA_DEVICE_NAME = "Portenta Machine Control";
};

#endif // MQTT_MANAGER_H
