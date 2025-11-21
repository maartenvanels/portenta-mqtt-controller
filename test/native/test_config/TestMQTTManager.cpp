#include <unity.h>
#include "Network/MQTTManager.h"
#include "PubSubClient.h"
#include "IO/IoController.h"

// Mock objects
extern PubSubClient* mockMqttClient;

void test_mqtt_publish_pin_state() {
    // 1. Arrange
    MQTTManager& mqtt = MQTTManager::getInstance();
    
    // Setup mock client
    Client mockClient;
    PubSubClient client(mockClient);
    mqtt.initialize(client);
    
    // Force connection state to allow publishing
    client.connect("test_client");
    
    // Create a dummy pin state (valid, value 1.0)
    IO::PinState state;
    state.value = 1.0f;
    state.isValid = true;
    
    // 2. Act
    // Publish state for pin 0 with topic "my_pin"
    mqtt.publishPinState(0, state, "my_pin");
    
    // 3. Assert
    // Verify topic structure matches "portenta/<topic>/state"
    TEST_ASSERT_EQUAL_STRING_MESSAGE("portenta/my_pin/state", client.lastTopic_.c_str(), "MQTT topic should follow standard format");
    
    // Verify payload formatting (3 decimal places)
    TEST_ASSERT_EQUAL_STRING_MESSAGE("1.000", client.lastPayload_.c_str(), "Payload should be formatted to 3 decimal places");
    
    // Verify retained flag
    TEST_ASSERT_TRUE_MESSAGE(client.lastRetained_, "State messages should be retained");
}
