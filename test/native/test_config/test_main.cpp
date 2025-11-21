#include <Arduino.h>
#include <Core/ConfigManager.h>
#include <unity.h>

void setUp(void)
{
    // Set up before each test
}

void tearDown(void)
{
    // Clean up after each test
}

void test_load_default_config()
{
    ConfigManager &config = ConfigManager::getInstance();
    bool result = config.loadDefaultConfig();

    TEST_ASSERT_TRUE(result);

    // Verify pins were loaded (should be 35 pins in default config)
    const auto &pins = config.getPins();
    TEST_ASSERT_TRUE(pins.size() > 0);
    TEST_ASSERT_EQUAL(35, pins.size());

    // Verify first pin (relay_0)
    const auto &pin0 = pins[0];
    TEST_ASSERT_EQUAL(0, pin0.pinNumber);
    TEST_ASSERT_EQUAL(IO::PinType::DIGITAL_OUTPUT, pin0.type);
    TEST_ASSERT_EQUAL_STRING("relay_0", pin0.name.c_str());
}

extern void test_mqtt_publish_pin_state();
extern void test_firmware_validation_valid();
extern void test_firmware_validation_invalid();

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_load_default_config);
    RUN_TEST(test_mqtt_publish_pin_state);
    RUN_TEST(test_firmware_validation_valid);
    RUN_TEST(test_firmware_validation_invalid);
    return UNITY_END();
}
