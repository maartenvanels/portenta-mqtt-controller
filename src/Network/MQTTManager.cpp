#include "Network/MQTTManager.h"
#include "Network/NetworkSettings.h"
#include "Core/credentials.h"
#include "Core/Logger.h"
#include "IO/IoController.h"
#include "Services/HomeAssistant/HomeAssistantDiscovery.h"
#include <Arduino_ConnectionHandler.h>
#include <ArduinoJson.h>

#if defined(BOARD_HAS_ETHERNET)
  #include <Ethernet.h>
#endif
#if defined(BOARD_HAS_WIFI)
  #include <WiFi.h>
#endif

// Static instance pointer for callback wrapper
static MQTTManager *s_instance = nullptr;

MQTTManager &MQTTManager::getInstance()
{
    static MQTTManager instance;
    return instance;
}

MQTTManager::MQTTManager()
    : mqttClient_(nullptr), messageCallback_(nullptr), lastConnectionAttempt_(0), lastConnectedState_(false)
{
    s_instance = this;
}

void MQTTManager::initialize(PubSubClient &client)
{
    mqttClient_ = &client;
    mqttClient_->setCallback(messageCallbackWrapper);
}

bool MQTTManager::connect(ConnectionHandler &connectionHandler, IO::IoController &ioController)
{
    if (!mqttClient_)
    {
        return false;
    }

    // Prevent rapid reconnection attempts
    uint32_t now = millis();
    if (now - lastConnectionAttempt_ < RECONNECT_INTERVAL_MS)
    {
        return false;
    }
    lastConnectionAttempt_ = now;

    // Get MQTT settings from NetworkSettings
    NetworkSettings &settings = NetworkSettings::getInstance();
    const NetworkSettings::MQTTConfig &mqttConfig = settings.getMQTTConfig();

    // Use stored MQTT settings or fallback to credentials.h
    const char *broker = mqttConfig.broker.length() > 0 ? mqttConfig.broker.c_str() : MQTT_BROKER;
    uint16_t port = mqttConfig.port > 0 ? mqttConfig.port : MQTT_PORT;
    const char *username = mqttConfig.username.length() > 0 ? mqttConfig.username.c_str() : MQTT_USERNAME;
    const char *password = mqttConfig.password.length() > 0 ? mqttConfig.password.c_str() : MQTT_PASSWORD;
    const char *clientId = mqttConfig.clientId.length() > 0 ? mqttConfig.clientId.c_str() : MQTT_CLIENT_ID;

    // Set server
    mqttClient_->setServer(broker, port);

    Serial.print("Connecting to MQTT broker: ");
    Serial.print(broker);
    Serial.print(":");
    Serial.println(port);
    Serial.print("Client ID: ");
    Serial.println(clientId);

    // Verify network connection before MQTT attempt
    if (connectionHandler.check() != NetworkConnectionState::CONNECTED)
    {
        Serial.println("ERROR: No network connection! Cannot connect to MQTT.");
        return false;
    }

    // Update the network client in PubSubClient (provided by Arduino ConnectionHandler)
#if defined(BOARD_HAS_LORA) || defined(BOARD_HAS_NOTECARD)
    Serial.println("ERROR: MQTT requires a TCP Client; current adapter does not provide Client()");
    return false;
#else
    mqttClient_->setClient(connectionHandler.getClient());
#endif

    Serial.print("IP address: ");
    if (connectionHandler.getInterface() == NetworkAdapter::ETHERNET)
    {
#if defined(BOARD_HAS_ETHERNET)
        Serial.println(Ethernet.localIP());
#else
        Serial.println("0.0.0.0");
#endif
    }
    else
    {
#if defined(BOARD_HAS_WIFI)
        Serial.println(WiFi.localIP());
#else
        Serial.println("0.0.0.0");
#endif
    }

    // Build LWT topic for Home Assistant availability
    char lwtTopic[128];
    snprintf(lwtTopic, sizeof(lwtTopic), "portenta/%s/availability", HA_DEVICE_ID);

    // Connect with LWT: if connection drops, broker publishes "offline" to availability topic
    if (mqttClient_->connect(clientId, username, password, lwtTopic, 0, true, "offline"))
    {
        Serial.println("Connected to MQTT broker!");

        // Only log on state change (disconnected -> connected)
        if (!lastConnectedState_)
        {
            LOG_INFO("MQTT connected to broker: " + String(broker) + ":" + String(port));
            lastConnectedState_ = true;
        }

        // Subscribe to control topics
        Serial.println("\n=== Subscribing to MQTT Topics ===");
        bool sub1 = mqttClient_->subscribe("portenta/+/set");
        Serial.print("  portenta/+/set -> ");
        Serial.println(sub1 ? "SUCCESS" : "FAILED");

        bool sub2 = mqttClient_->subscribe("portenta/config");
        Serial.print("  portenta/config -> ");
        Serial.println(sub2 ? "SUCCESS" : "FAILED");

        if (sub1 && sub2)
        {
            Serial.println("All subscriptions successful!");
        }
        else
        {
            Serial.println("WARNING: Some subscriptions failed!");
            LOG_WARNING("MQTT: Some subscriptions failed!");
        }

        // Publish online status
        mqttClient_->publish("portenta/status", "online", true);
        Serial.println("Published online status");

        // Publish Home Assistant Discovery
        HA::HADiscovery::publishDiscovery(*mqttClient_, ioController, HA_DEVICE_ID, HA_DEVICE_NAME);

        // Publish availability for Home Assistant
        HA::HADiscovery::publishAvailability(*mqttClient_, HA_DEVICE_ID, true);

        return true;
    }
    else
    {
        Serial.print("MQTT connection failed, rc=");
        Serial.println(mqttClient_->state());
        Serial.println("Reason: -1=wrong protocol, -2=invalid client id, -3=unavailable, -4=bad credentials, -5=not authorized");

        // Only log on state change (connected -> disconnected)
        if (lastConnectedState_)
        {
            LOG_ERROR("MQTT connection lost, rc=" + String(mqttClient_->state()));
            lastConnectedState_ = false;
        }

        return false;
    }
}

bool MQTTManager::isConnected()
{
    return mqttClient_ && mqttClient_->connected();
}

void MQTTManager::loop()
{
    if (mqttClient_)
    {
        mqttClient_->loop();
    }
}

void MQTTManager::publishPinState(uint8_t pin, const IO::PinState &state, const std::string &topic)
{
    if (!isConnected() || topic.empty())
    {
        return;
    }

    String fullTopic = "portenta/" + String(topic.c_str()) + "/state";
    String payload = String(state.value, 3);

    mqttClient_->publish(fullTopic.c_str(), payload.c_str(), true); // retained=true
}

void MQTTManager::publishAllPinStates(IO::IoController &ioController)
{
    if (!isConnected())
    {
        return;
    }

    // Get all pin handlers
    const auto pinNumbers = ioController.getAllPinNumbers();

    Serial.print("Publishing states for ");
    Serial.print(pinNumbers.size());
    Serial.println(" pins...");

    int inputCount = 0;
    int outputCount = 0;

    for (uint16_t key : pinNumbers)
    {
        IO::IPinHandler *handler = ioController.getPin(key);
        if (handler)
        {
            const auto &config = handler->getConfiguration();
            const auto &state = handler->getState();

            // Publish INPUT pins (always, even if not valid yet - will show 0.0)
            if (config.mode == IO::PinMode::INPUT)
            {
                inputCount++;
                publishPinState(key & 0xFF, state, config.mqttTopic);
            }
            // Also publish OUTPUT states so we know their current value
            else if (config.mode == IO::PinMode::OUTPUT)
            {
                outputCount++;
                publishPinState(key & 0xFF, state, config.mqttTopic);
            }
        }
    }

    Serial.print("Published ");
    Serial.print(inputCount);
    Serial.print(" inputs, ");
    Serial.print(outputCount);
    Serial.println(" outputs");
}

void MQTTManager::publishSystemStatus()
{
    if (!isConnected())
    {
        return;
    }

    IO::IoController &ioController = IO::IoController::getInstance();

    // Create status JSON
    const auto pinNumbers = ioController.getAllPinNumbers();
    const size_t capacity = 256 + pinNumbers.size() * 128;
    ArduinoJson::DynamicJsonDocument status(capacity);
    status["uptime"] = millis();
#if defined(ARDUINO_ARCH_ESP32)
    status["freeHeap"] = ESP.getFreeHeap();
#else
    status["freeHeap"] = 0;
#endif
#ifdef USE_ETHERNET
    status["network"] = "ethernet";
    // Ethernet doesn't have RSSI
#else
    status["network"] = "wifi";
#if defined(BOARD_HAS_WIFI)
    status["rssi"] = WiFi.RSSI();
#endif
#endif
    status["ioHealth"] = ioController.isHealthy();

    // Add pin summary
    ArduinoJson::JsonArray pins = status.createNestedArray("pins");
    for (uint8_t pinNum : pinNumbers)
    {
        IO::IPinHandler *pin = ioController.getPin(pinNum);
        if (pin)
        {
            ArduinoJson::JsonObject pinObj = pins.createNestedObject();
            pinObj["pin"] = pinNum;
            pinObj["name"] = pin->getConfiguration().name;
            pinObj["value"] = pin->getState().value;
            pinObj["valid"] = pin->getState().isValid;
            if (!pin->getConfiguration().unit.empty())
            {
                pinObj["unit"] = pin->getConfiguration().unit;
            }
        }
    }

    // Serialize and publish
    String statusJson;
    serializeJson(status, statusJson);
    mqttClient_->publish("portenta/status/detail", statusJson.c_str());
}

void MQTTManager::setMessageCallback(MessageCallback callback)
{
    messageCallback_ = callback;
}

int MQTTManager::getState()
{
    return mqttClient_ ? mqttClient_->state() : -1;
}

void MQTTManager::messageCallbackWrapper(char *topic, byte *payload, unsigned int length)
{
    if (s_instance && s_instance->messageCallback_)
    {
        s_instance->messageCallback_(topic, payload, length);
    }
}
