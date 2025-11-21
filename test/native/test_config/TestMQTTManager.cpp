#include <unity.h>
#include "Network/MQTTManager.h"
#include "PubSubClient.h"
#include "IO/IoController.h"

// Mock objects
extern PubSubClient* mockMqttClient;

void test_mqtt_publish_pin_state() {
    MQTTManager& mqtt = MQTTManager::getInstance();
    
    // Setup mock client
    Client mockClient;
    PubSubClient client(mockClient);
    mqtt.initialize(client);
    
    // Force connection state
    client.connect("test_client");
    
    // Create a dummy pin state
    IO::PinState state;
    state.value = 1.0f;
    state.isValid = true;
    
    // Publish
    mqtt.publishPinState(0, state, "my_pin");
    
    // Verify
    TEST_ASSERT_EQUAL_STRING("portenta/my_pin/state", client.lastTopic_.c_str());
    TEST_ASSERT_EQUAL_STRING("1.000", client.lastPayload_.c_str());
    TEST_ASSERT_TRUE(client.lastRetained_);
}
