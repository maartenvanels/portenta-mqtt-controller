#include <Arduino.h>
#include <rtos.h>
#undef abs
#include <Arduino_PortentaMachineControl.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <string>
#include <BlockDevice.h>
#include <MBRBlockDevice.h>
#include <FATFileSystem.h>

#include "Core/credentials.h"
#include "Core/ConfigManager.h"
#include "Network/NetworkSettings.h"
#include "Network/NetworkManager.h"
#include "Core/TimeManager.h"
#include "Network/MQTTManager.h"
#include "Core/SystemDiagnostics.h"
#include "Hardware/MachineControlHAL.h"
#include "IO/IoController.h"
#include "Services/HomeAssistant/HomeAssistantDiscovery.h"
#include "Network/WebServer.h"
#include "Core/Logger.h"
#include <Arduino_UnifiedStorage.h>

const char *HA_DEVICE_ID = "portenta_h7";                // Unique device identifier
const char *HA_DEVICE_NAME = "Portenta Machine Control"; // Human-readable name

// Global objects - Network and MQTT connectivity
NetworkManager &networkManager = NetworkManager::getInstance();
PubSubClient mqttClient(networkManager.getClient());
MQTTManager &mqttManager = MQTTManager::getInstance();

// Note: MQTT buffer size is set in platformio.ini (MQTT_MAX_PACKET_SIZE=1024)
// This prevents connection issues with large messages

HAL::MachineControlHAL &hal = HAL::MachineControlHAL::getInstance();
IO::IoController &ioController = IO::IoController::getInstance();
Web::WebServer &webServer = Web::WebServer::getInstance();

// System state
enum class SystemMode
{
    STARTUP,
    CONNECTING,
    OPERATIONAL,
    ERROR
};

SystemMode systemMode = SystemMode::STARTUP;
uint32_t lastStatusUpdate = 0;
uint32_t lastProcessTime = 0;
uint32_t lastPeriodicPublish = 0; // For periodic state publishing

// RTOS Threads
// rtos::Thread webServerThread(osPriorityBelowNormal, 32768); // 32KB stack for WebServer

// Function declarations
// void webServerTask();
void handleMqttMessage(const char *topic, const byte *payload, unsigned int length);
void setupDefaultPins();
void loadConfigFromFile();
void reloadConfiguration(); // Hot-reload configuration without restart

void setup()
{
    // Initialize serial for debugging
    Serial.begin(115200);
    while (!Serial && millis() < 5000)
    {
        delay(10);
    }

    Serial.println("Arduino Portenta Machine Control MQTT Controller");
    Serial.println("Initializing...");

    // Run OTA diagnostics at boot
    SystemDiagnostics::runOtaDiagnostics();

    // Initialize Logger (early, so all subsequent init can log)
    Logging::Logger &logger = Logging::Logger::getInstance();
    if (logger.initialize(500))
    { // 500 KB max log file
        Serial.println("Logger initialized successfully");
        logger.setLogLevel(Logging::LogLevel::INFO); // Default to INFO level
        LOG_INFO("=== SYSTEM BOOT ===");
        LOG_INFO("Portenta Machine Control MQTT Controller starting");
    }
    else
    {
        Serial.println("WARNING: Logger initialization failed - logs will not persist");
    }

    // Set MQTT buffer size (must be done before first use)
    mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
    Serial.print("MQTT buffer size set to: ");
    Serial.println(MQTT_MAX_PACKET_SIZE);
    LOG_INFO("MQTT buffer size: " + String(MQTT_MAX_PACKET_SIZE));

    // Initialize Machine Control hardware
    if (!hal.initialize())
    {
        Serial.println("ERROR: Failed to initialize Machine Control hardware!");
        LOG_CRITICAL("Machine Control hardware initialization FAILED");
        systemMode = SystemMode::ERROR;
        return;
    }

    Serial.println("Machine Control hardware initialized");
    LOG_INFO("Machine Control hardware initialized");

    // Initialize I/O controller
    if (!ioController.initialize())
    {
        Serial.println("ERROR: Failed to initialize I/O controller!");
        LOG_CRITICAL("I/O controller initialization FAILED");
        systemMode = SystemMode::ERROR;
        return;
    }

    Serial.println("I/O controller initialized");
    LOG_INFO("I/O controller initialized");

    // Enable Watchdog (30 second timeout)
    // Note: mbed::Watchdog is a singleton.
    if (mbed::Watchdog::get_instance().start(30000))
    {
        Serial.println("Watchdog timer started (30s timeout)");
        LOG_INFO("Watchdog timer started");
    }
    else
    {
        Serial.println("WARNING: Watchdog timer failed to start");
        LOG_WARNING("Watchdog timer failed to start");
    }

    // Initialize MQTT Manager
    mqttManager.initialize(mqttClient);
    mqttManager.setMessageCallback(handleMqttMessage);

    // Set up global state change callback for MQTT publishing
    ioController.setGlobalStateChangeCallback(
        [](uint8_t pin, const IO::PinState &state, const std::string &topic)
        {
            mqttManager.publishPinState(pin, state, topic);
        });

    // Load network settings from flash
    NetworkSettings &networkSettings = NetworkSettings::getInstance();
    Serial.println("\n=== Loading Network Settings ===");
    if (networkSettings.loadSettings())
    {
        Serial.println("Network settings loaded from flash");
        LOG_INFO("Network settings loaded from flash");
    }
    else
    {
        Serial.println("Using default network settings");
        LOG_WARNING("Using default network settings - flash load failed");
    }

    // Connect to network
    Serial.println("\n=== Connecting to Network ===");
    Serial.println("Prioritizing Ethernet, falling back to WiFi");
    LOG_INFO("Network mode: Auto (Ethernet/WiFi)");

    systemMode = SystemMode::CONNECTING;

    // Start connection process (async)
    if (networkManager.connect())
    {
        Serial.println("Network initialization started...");
    }
    else
    {
        Serial.println("WARNING: Network initialization failed! Will retry in loop.");
        // Fallback to CONNECTING mode so loop() keeps retrying
        systemMode = SystemMode::CONNECTING;
    }

    // Load pin configuration (from config.json or hardcoded defaults)
    loadConfigFromFile();

    // Web Server initialization moved to loop() when network connects
    // webServer.setIoController(&ioController);
    // webServer.initialize(WEB_PORT);

    Serial.println("\nSetup complete");
    LOG_INFO("System initialization complete");
}

void loop()
{
    // Feed the watchdog
    mbed::Watchdog::get_instance().kick();

    uint32_t now = millis();
    uint32_t t0;

    // Process I/O at configured rate
    t0 = micros();
    if (now - lastProcessTime >= 10)
    { // 100Hz max processing rate
        lastProcessTime = now;
        ioController.process();
    }
    SystemDiagnostics::recordTask(SystemDiagnostics::TaskType::IO, micros() - t0);

    // Handle different system modes
    switch (systemMode)
    {
    case SystemMode::CONNECTING:
        // Monitor network connection
        t0 = micros();
        networkManager.maintain();
        SystemDiagnostics::recordTask(SystemDiagnostics::TaskType::MQTT, micros() - t0);

        // Check if network is connected
        if (networkManager.isConnected())
        {
            Serial.println("Network connected!");
            Serial.print("IP address: ");
            Serial.println(networkManager.getIPAddress());

            // Connect MQTT once network is ready
            mqttManager.connect(ioController);

            // Initialize TimeManager
            TimeManager::getInstance().begin();

            // Initialize Web Server now that we have an IP
            Serial.println("Initializing Web Server...");
            webServer.setIoController(&ioController);
            webServer.initialize(WEB_PORT);

            systemMode = SystemMode::OPERATIONAL;
        }
        else if (now - lastStatusUpdate > 5000)
        {
            Serial.println("Waiting for network connection...");
            lastStatusUpdate = now;
        }
        break;

    case SystemMode::OPERATIONAL:
        // Monitor network health and recover if needed
        t0 = micros();
        networkManager.maintain();
        SystemDiagnostics::recordTask(SystemDiagnostics::TaskType::MQTT, micros() - t0);

        // Maintain MQTT connection
        t0 = micros();
        if (networkManager.isConnected())
        {
            // Update TimeManager
            TimeManager::getInstance().update();

            static uint32_t lastMqttAttempt = 0;
            bool mqttConnected = mqttManager.isConnected();

            // Update WebServer status
            webServer.setMqttConnected(mqttConnected);

            if (!mqttConnected)
            {
                if (now - lastMqttAttempt > 5000)
                {
                    lastMqttAttempt = now;
                    mqttManager.connect(ioController);
                }
            }
            else
            {
                mqttManager.loop();
            }
        }
        SystemDiagnostics::recordTask(SystemDiagnostics::TaskType::MQTT, micros() - t0);

        // Handle web server clients
        t0 = micros();
        webServer.handleClient();
        SystemDiagnostics::recordTask(SystemDiagnostics::TaskType::WEB, micros() - t0);

        // Publish all pin states periodically
        if (now - lastPeriodicPublish > 5000)
        { // Every 5 seconds
            lastPeriodicPublish = now;
            mqttManager.publishAllPinStates(ioController);
        }
        if (now - lastStatusUpdate > 10000)
        { // Every 10 seconds
            lastStatusUpdate = now;
            mqttManager.publishSystemStatus();

            // Debug: Show that device is alive and MQTT is connected
            Serial.println("\n--- HEARTBEAT ---");
            Serial.print("Network: ");
            Serial.println(networkManager.isConnected() ? "CONNECTED" : "DISCONNECTED");
            Serial.print("IP: ");
            Serial.println(networkManager.getIPAddress());
            Serial.print("MQTT Status: ");
            Serial.println(mqttManager.isConnected() ? "CONNECTED" : "DISCONNECTED");
            Serial.print("Time: ");
            Serial.println(TimeManager::getInstance().getFormattedTime());
            Serial.println("----------------");
        }
        break;

    case SystemMode::ERROR:
        // Flash error LED pattern
        static uint32_t lastBlink = 0;
        if (now - lastBlink > 250)
        {
            lastBlink = now;
            // TODO: Toggle error LED
        }
        break;

    default:
        break;
    }

    // Check for config reload request from WebServer
    if (ConfigManager::getInstance().shouldReload())
    {
        ConfigManager::getInstance().setShouldReload(false);
        reloadConfiguration();
    }

    // Yield to OS scheduler to reduce CPU load
    // Using delay(10) ensures ~100Hz loop rate and allows idle thread to run
    delay(10);
}

// Network and MQTT management functions moved to NetworkManager and MQTTManager classes

void handleMqttMessage(const char *topic, const byte *payload, unsigned int length)
{
    Serial.println("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
    Serial.println(">>> MQTT CALLBACK TRIGGERED <<<");
    Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");

    // Convert payload to string
    std::string message(reinterpret_cast<const char *>(payload), reinterpret_cast<const char *>(payload) + length);

    Serial.print("MQTT message received [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.print(length);
    Serial.println(" bytes");

    // Only print full message if not too long
    if (length < 150)
    {
        Serial.print("Payload: ");
        Serial.println(message.c_str());
    }
    else
    {
        Serial.print("Payload preview (first 100 chars): ");
        Serial.println(String(message.c_str()).substring(0, 100));
    }

    String topicStr = String(topic);

    // ===== CONFIG UPDATE =====
    if (topicStr == "portenta/config")
    {
        Serial.println("\n=== Updating configuration from MQTT ===");
        Serial.print("Payload length: ");
        Serial.println(length);
        Serial.print("MQTT connected: ");
        Serial.println(mqttClient.connected() ? "YES" : "NO");
        LOG_INFO("MQTT config update received (" + String(length) + " bytes)");

        ConfigManager &configMgr = ConfigManager::getInstance();

        String jsonPayload(message.c_str());
        if (configMgr.loadFromJson(jsonPayload))
        {
            Serial.println("Configuration JSON validated successfully!");
            LOG_INFO("Config JSON validated - applying hot-reload");

            // Send acknowledgment via MQTT
            Serial.println("Publishing status: validating");
            bool pub1 = mqttClient.publish("portenta/config/status", "validating", false);
            Serial.print("  Publish result: ");
            Serial.println(pub1 ? "OK" : "FAILED");

            Serial.println("Applying new configuration with hot-reload...");
            reloadConfiguration();
            Serial.println("Configuration applied! No restart required.");

            // Send success confirmation via MQTT
            String successMsg = "success: " + String(ioController.getAllPinNumbers().size()) + " pins active";
            Serial.print("Publishing status: ");
            Serial.println(successMsg);
            bool pub2 = mqttClient.publish("portenta/config/status", successMsg.c_str(), false);
            Serial.print("  Publish result: ");
            Serial.println(pub2 ? "OK" : "FAILED");
            LOG_INFO("Config applied successfully via MQTT");
        }
        else
        {
            Serial.println("Configuration update failed - invalid JSON or validation error");
            LOG_ERROR("Config update FAILED - invalid JSON");

            // Send error via MQTT
            Serial.println("Publishing status: error");
            bool pub3 = mqttClient.publish("portenta/config/status", "error: invalid JSON", false);
            Serial.print("  Publish result: ");
            Serial.println(pub3 ? "OK" : "FAILED");
        }
        return;
    }

    // ===== PIN CONTROL =====
    if (topicStr.startsWith("portenta/") && topicStr.endsWith("/set"))
    {
        // Extract pin name/topic
        int start = topicStr.indexOf('/') + 1;
        int end = topicStr.lastIndexOf('/');
        String pinTopic = topicStr.substring(start, end);

        // Find pin by topic
        IO::IPinHandler *pin = ioController.getPinByTopic(pinTopic.c_str());
        if (pin)
        {
            // Parse value
            float value = atof(message.c_str());
            if (pin->setValue(value))
            {
                Serial.print("Set pin value: ");
                Serial.println(value);
            }
            else
            {
                Serial.println("Failed to set pin value");
            }
        }
    }
}

// MQTT publish functions moved to MQTTManager class

void loadConfigFromFile()
{
    Serial.println("\n=== Loading Pin Configuration ===");
    LOG_INFO("Loading pin configuration from storage");

    ConfigManager &configMgr = ConfigManager::getInstance();

    // Try to load from storage first, fallback to defaults
    if (!configMgr.loadFromStorage())
    {
        Serial.println("Storage load failed, loading defaults...");
        LOG_WARNING("Config storage load failed - using defaults");
        configMgr.loadDefaultConfig();
    }

    // Load pins from config
    const auto &pins = configMgr.getPins();
    Serial.print("Total pins to configure: ");
    Serial.println(pins.size());
    LOG_INFO("Configuring " + String(pins.size()) + " pins");

    int successCount = 0;
    int failCount = 0;

    for (const auto &cfg : pins)
    {
        if (ioController.addPin(cfg))
        {
            Serial.print("OK: ");
            Serial.println(cfg.name.c_str());
            successCount++;
        }
        else
        {
            Serial.print("FAIL: ");
            Serial.println(cfg.name.c_str());
            LOG_ERROR("Failed to add pin: " + String(cfg.name.c_str()));
            failCount++;
        }
    }

    Serial.println("Pin configuration complete\n");
    LOG_INFO("Pin config complete: " + String(successCount) + " OK, " + String(failCount) + " failed");
}

// setupDefaultPins removed - configuration loaded from config.json instead

void setupDefaultPins()
{
    Serial.println("\n=== Configuring All I/O Pins ===\n");

    // ===== DIGITAL OUTPUTS (All 8 Relays) =====
    Serial.println("--- Digital Outputs (DO0-DO7) ---");
    for (uint8_t i = 0; i < 8; i++)
    {
        IO::PinConfiguration cfg;
        cfg.pinNumber = i;
        cfg.type = IO::PinType::DIGITAL_OUTPUT;
        cfg.mode = IO::PinMode::OUTPUT;
        char nameBuf[32];
        sprintf(nameBuf, "relay_%d", i + 1);
        cfg.name = nameBuf;
        sprintf(nameBuf, "relay%d", i + 1);
        cfg.mqttTopic = nameBuf;

        if (ioController.addPin(cfg))
        {
            Serial.print("OK: ");
            Serial.print(cfg.name.c_str());
            Serial.print(" (DO");
            Serial.print(i);
            Serial.print(") -> portenta/");
            Serial.print(cfg.mqttTopic.c_str());
            Serial.println("/set");
        }
        else
        {
            Serial.print("ERROR: Failed to add relay ");
            Serial.println(i + 1);
        }
    }

    // ===== PROGRAMMABLE DI/DO (Channels 0-5, configure as INPUTS) =====
    // Note: Using programmable channels 0-5 for digital inputs
    Serial.println("\n--- Programmable DI/DO Inputs (PROG_IN 0-5) ---");
    for (uint8_t i = 0; i < 6; i++)
    {
        IO::PinConfiguration cfg;
        cfg.pinNumber = i;
        cfg.type = IO::PinType::PROGRAMMABLE_DIO;
        cfg.mode = IO::PinMode::INPUT;
        char nameBuf[32];
        sprintf(nameBuf, "prog_in_%d", i + 1);
        cfg.name = nameBuf;
        sprintf(nameBuf, "prog_in%d", i + 1);
        cfg.mqttTopic = nameBuf;
        cfg.sampleRateMs = 100; // Sample every 100ms

        if (ioController.addPin(cfg))
        {
            Serial.print("OK: ");
            Serial.print(cfg.name.c_str());
            Serial.print(" (PROG_IN");
            Serial.print(i);
            Serial.print(") -> portenta/");
            Serial.print(cfg.mqttTopic.c_str());
            Serial.println("/state");
        }
        else
        {
            Serial.print("SKIP: Programmable input ");
            Serial.println(i);
        }
    }

    // ===== PROGRAMMABLE DI/DO (Channels 6-11, configure as outputs) =====
    // Note: Channels 0-5 used for inputs, 6-11 for outputs
    Serial.println("\n--- Programmable DI/DO Outputs (PROG_OUT 6-11) ---");
    for (uint8_t i = 6; i < 12; i++)
    {
        IO::PinConfiguration cfg;
        cfg.pinNumber = i;
        cfg.type = IO::PinType::PROGRAMMABLE_DIO;
        cfg.mode = IO::PinMode::OUTPUT;
        char nameBuf[32];
        sprintf(nameBuf, "prog_out_%d", i - 5); // Number from 1-6
        cfg.name = nameBuf;
        sprintf(nameBuf, "prog_out%d", i - 5);
        cfg.mqttTopic = nameBuf;

        if (ioController.addPin(cfg))
        {
            Serial.print("OK: ");
            Serial.print(cfg.name.c_str());
            Serial.print(" (PROG");
            Serial.print(i);
            Serial.print(") -> portenta/");
            Serial.print(cfg.mqttTopic.c_str());
            Serial.println("/set");
        }
        else
        {
            Serial.print("SKIP: Programmable output ");
            Serial.println(i);
        }
    }

    Serial.println("\n=== Pin Configuration Complete ===");
    Serial.println("MQTT CONTROL Topics (Publish to these):");
    Serial.println("  Relays:          portenta/relay1-8/set (0 or 1)");
    Serial.println("  Prog Outputs:    portenta/prog_out1-6/set (0 or 1)");
    Serial.println("\nMQTT STATE Topics (Subscribe to these):");
    Serial.println("  Prog Inputs:     portenta/prog_in1-6/state");
    Serial.println("  Prog Outputs:    portenta/prog_out1-6/state");
    Serial.println("\nExample MQTT commands:");
    Serial.println("  mosquitto_pub -h 192.168.18.74 -t portenta/relay1/set -m \"1\"");
    Serial.println("  mosquitto_pub -h 192.168.18.74 -t portenta/prog_out1/set -m \"1\"\n");
}

void reloadConfiguration()
{
    Serial.println("\n=== Hot-Reloading Configuration ===");
    LOG_INFO("=== Configuration Hot-Reload Started ===");

    // Step 1: Shutdown existing I/O controller (stops all pin handlers)
    Serial.println("Step 1: Shutting down existing pin handlers...");
    ioController.shutdown();
    Serial.println("  All pins stopped");
    LOG_INFO("Step 1: All pin handlers stopped");

    // Step 2: Re-initialize I/O controller
    Serial.println("Step 2: Re-initializing I/O controller...");
    if (!ioController.initialize())
    {
        Serial.println("  ERROR: Failed to re-initialize I/O controller!");
        LOG_ERROR("Step 2: I/O controller re-initialization FAILED");
        return;
    }
    Serial.println("  I/O controller ready");
    LOG_INFO("Step 2: I/O controller re-initialized");

    // Step 3: Restore global state change callback for MQTT publishing
    Serial.println("Step 3: Restoring MQTT callback...");
    ioController.setGlobalStateChangeCallback(
        [](uint8_t pin, const IO::PinState &state, const std::string &topic)
        {
            mqttManager.publishPinState(pin, state, topic);
        });
    Serial.println("  Callback restored");

    // Step 4: Load pins from updated configuration
    Serial.println("Step 4: Loading new pin configuration...");
    ConfigManager &configMgr = ConfigManager::getInstance();
    const auto &pins = configMgr.getPins();

    int successCount = 0;
    int failCount = 0;

    for (const auto &cfg : pins)
    {
        if (ioController.addPin(cfg))
        {
            successCount++;
        }
        else
        {
            Serial.print("  FAIL: ");
            Serial.println(cfg.name.c_str());
            failCount++;
        }
    }

    Serial.print("  Loaded ");
    Serial.print(successCount);
    Serial.print(" pins (");
    Serial.print(failCount);
    Serial.println(" failed)");
    LOG_INFO("Step 4: Loaded " + String(successCount) + " pins (" + String(failCount) + " failed)");

    // Step 5: Re-publish Home Assistant Discovery
    if (mqttClient.connected())
    {
        Serial.println("Step 5: Updating Home Assistant Discovery...");
        HA::HADiscovery::publishDiscovery(mqttClient, ioController, HA_DEVICE_ID, HA_DEVICE_NAME);
        Serial.println("  Discovery updated");
        LOG_INFO("Step 5: Home Assistant Discovery updated");

        // Step 6: Publish initial states for all new pins
        Serial.println("Step 6: Publishing initial pin states...");
        mqttManager.publishAllPinStates(ioController);
        Serial.println("  Initial states published");
        LOG_INFO("Step 6: Initial pin states published");
    }
    else
    {
        Serial.println("Step 5: MQTT not connected, skipping Discovery update");
        LOG_WARNING("Step 5: MQTT not connected - skipping Discovery");
    }

    Serial.println("\n=== Configuration Reload Complete ===");
    Serial.print("Total pins active: ");
    Serial.println(ioController.getAllPinNumbers().size());
    LOG_INFO("=== Configuration Hot-Reload Complete === Total: " + String(ioController.getAllPinNumbers().size()) + " pins");
}
