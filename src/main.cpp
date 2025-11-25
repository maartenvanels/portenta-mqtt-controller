/**
 * MVP: Pure Arduino Cloud Implementation
 *
 * This is a minimal implementation using ONLY:
 * - WiFiConnectionHandler for network management
 * - ArduinoIoTCloud for cloud connectivity
 *
 * No NetworkManager, no MQTT, no WebServer - just cloud.
 */

#include <Arduino.h>
#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include "Core/credentials.h"  // NEVER commit this file - it's gitignored

// ============================================================================
// CLOUD PROPERTIES - Names must match Arduino Cloud Thing properties!
// ============================================================================

bool example_switch = false;
float example_value = 0.0;

// Callback when example_switch changes from cloud
void onExampleSwitchChange() {
    Serial.print("[Cloud] example_switch changed to: ");
    Serial.println(example_switch ? "ON" : "OFF");
}

// ============================================================================
// CONNECTION HANDLER
// ============================================================================

WiFiConnectionHandler connectionHandler(WIFI_SSID, WIFI_PASSWORD);

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);

    // Wait for serial (max 5 seconds)
    unsigned long startWait = millis();
    while (!Serial && millis() - startWait < 5000) {
        delay(10);
    }

    Serial.println("\n\n========================================");
    Serial.println("MVP: Pure Arduino Cloud Implementation");
    Serial.println("========================================\n");

    // Setup cloud credentials
    Serial.println("[Setup] Configuring Arduino Cloud...");
    ArduinoCloud.setThingId(CLOUD_THING_ID);
    ArduinoCloud.setDeviceId(CLOUD_DEVICE_ID);
    ArduinoCloud.setSecretDeviceKey(CLOUD_SECRET_KEY);

    // Register cloud properties
    // API: addProperty(variable, permission, policy, callback)
    Serial.println("[Setup] Registering properties...");
    ArduinoCloud.addProperty(example_switch, READWRITE, ON_CHANGE, onExampleSwitchChange);
    ArduinoCloud.addProperty(example_value, READ, ON_CHANGE);

    // Begin cloud connection
    Serial.println("[Setup] Starting ArduinoCloud.begin()...");
    ArduinoCloud.begin(connectionHandler);

    // Enable debug output
    setDebugMessageLevel(DBG_INFO);
    ArduinoCloud.printDebugInfo();

    Serial.println("[Setup] Setup complete!\n");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    // Update Arduino Cloud - this handles both WiFi and Cloud connection
    ArduinoCloud.update();

    // Status output every 5 seconds
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 5000) {
        lastStatus = millis();

        Serial.print("[Status] WiFi=");
        Serial.print(WiFi.status());
        Serial.print(" Cloud=");
        Serial.print(ArduinoCloud.connected() ? "YES" : "NO");
        Serial.print(" example_switch=");
        Serial.print(example_switch ? "ON" : "OFF");
        Serial.print(" example_value=");
        Serial.println(example_value);
    }

    // Simulate a changing value (for testing cloud sync)
    static unsigned long lastValueUpdate = 0;
    if (millis() - lastValueUpdate > 10000) {
        lastValueUpdate = millis();
        example_value = (float)(millis() / 1000);  // Seconds since boot
        Serial.print("[Local] Updated example_value to: ");
        Serial.println(example_value);
    }
}
