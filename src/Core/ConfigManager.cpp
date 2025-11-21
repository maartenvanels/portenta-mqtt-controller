#include "Core/ConfigManager.h"
#include "Core/Logger.h"
#include <Arduino.h>
#include <Arduino_UnifiedStorage.h>

// Embedded config.json content (will be compiled in)
const char PROGMEM DEFAULT_CONFIG[] = "{\"version\":1,\"pins\":[{\"pinNumber\":0,\"type\":\"DIGITAL_OUTPUT\",\"name\":\"relay_0\",\"mqttTopic\":\"relay0\",\"mode\":\"OUTPUT\"},{\"pinNumber\":1,\"type\":\"DIGITAL_OUTPUT\",\"name\":\"relay_1\",\"mqttTopic\":\"relay1\",\"mode\":\"OUTPUT\"},{\"pinNumber\":2,\"type\":\"DIGITAL_OUTPUT\",\"name\":\"relay_2\",\"mqttTopic\":\"relay2\",\"mode\":\"OUTPUT\"},{\"pinNumber\":3,\"type\":\"DIGITAL_OUTPUT\",\"name\":\"relay_3\",\"mqttTopic\":\"relay3\",\"mode\":\"OUTPUT\"},{\"pinNumber\":4,\"type\":\"DIGITAL_OUTPUT\",\"name\":\"relay_4\",\"mqttTopic\":\"relay4\",\"mode\":\"OUTPUT\"},{\"pinNumber\":5,\"type\":\"DIGITAL_OUTPUT\",\"name\":\"relay_5\",\"mqttTopic\":\"relay5\",\"mode\":\"OUTPUT\"},{\"pinNumber\":6,\"type\":\"DIGITAL_OUTPUT\",\"name\":\"relay_6\",\"mqttTopic\":\"relay6\",\"mode\":\"OUTPUT\"},{\"pinNumber\":7,\"type\":\"DIGITAL_OUTPUT\",\"name\":\"relay_7\",\"mqttTopic\":\"relay7\",\"mode\":\"OUTPUT\"},{\"pinNumber\":0,\"type\":\"DIGITAL_INPUT\",\"name\":\"digital_in_1\",\"mqttTopic\":\"digital_in1\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":1,\"type\":\"DIGITAL_INPUT\",\"name\":\"digital_in_2\",\"mqttTopic\":\"digital_in2\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":2,\"type\":\"DIGITAL_INPUT\",\"name\":\"digital_in_3\",\"mqttTopic\":\"digital_in3\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":3,\"type\":\"DIGITAL_INPUT\",\"name\":\"digital_in_4\",\"mqttTopic\":\"digital_in4\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":4,\"type\":\"DIGITAL_INPUT\",\"name\":\"digital_in_5\",\"mqttTopic\":\"digital_in5\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":5,\"type\":\"DIGITAL_INPUT\",\"name\":\"digital_in_6\",\"mqttTopic\":\"digital_in6\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":6,\"type\":\"DIGITAL_INPUT\",\"name\":\"digital_in_7\",\"mqttTopic\":\"digital_in7\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":7,\"type\":\"DIGITAL_INPUT\",\"name\":\"digital_in_8\",\"mqttTopic\":\"digital_in8\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":0,\"type\":\"ANALOG_INPUT_VOLTAGE\",\"name\":\"analog_voltage_1\",\"mqttTopic\":\"analog_in0\",\"mode\":\"INPUT\",\"sampleRateMs\":100,\"minValue\":0.0,\"maxValue\":1000.0,\"scaleFactor\":1.0,\"offset\":0.0,\"precision\":0,\"unit\":\"Lux\",\"lookupTable\":[{\"input\":0.0,\"output\":0.0},{\"input\":2.0,\"output\":100.0},{\"input\":4.0,\"output\":300.0},{\"input\":6.0,\"output\":600.0},{\"input\":10.0,\"output\":1000.0}]},{\"pinNumber\":1,\"type\":\"ANALOG_INPUT_VOLTAGE\",\"name\":\"analog_voltage_2\",\"mqttTopic\":\"analog_in1\",\"mode\":\"INPUT\",\"sampleRateMs\":100,\"minValue\":0.0,\"maxValue\":10.0,\"scaleFactor\":1.0,\"offset\":0.0},{\"pinNumber\":2,\"type\":\"ANALOG_INPUT_VOLTAGE\",\"name\":\"analog_voltage_3\",\"mqttTopic\":\"analog_in2\",\"mode\":\"INPUT\",\"sampleRateMs\":100,\"minValue\":0.0,\"maxValue\":10.0,\"scaleFactor\":1.0,\"offset\":0.0},{\"pinNumber\":0,\"type\":\"PROGRAMMABLE_DIO\",\"name\":\"prog_dio_0\",\"mqttTopic\":\"prog_dio_0\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":1,\"type\":\"PROGRAMMABLE_DIO\",\"name\":\"prog_dio_1\",\"mqttTopic\":\"prog_dio_1\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":2,\"type\":\"PROGRAMMABLE_DIO\",\"name\":\"prog_dio_2\",\"mqttTopic\":\"prog_dio_2\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":3,\"type\":\"PROGRAMMABLE_DIO\",\"name\":\"prog_dio_3\",\"mqttTopic\":\"prog_dio_3\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":4,\"type\":\"PROGRAMMABLE_DIO\",\"name\":\"prog_dio_4\",\"mqttTopic\":\"prog_dio_4\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":5,\"type\":\"PROGRAMMABLE_DIO\",\"name\":\"prog_dio_5\",\"mqttTopic\":\"prog_dio_5\",\"mode\":\"INPUT\",\"sampleRateMs\":100},{\"pinNumber\":6,\"type\":\"PROGRAMMABLE_DIO\",\"name\":\"prog_dio_6\",\"mqttTopic\":\"prog_dio_6\",\"mode\":\"OUTPUT\"},{\"pinNumber\":7,\"type\":\"PROGRAMMABLE_DIO\",\"name\":\"prog_dio_7\",\"mqttTopic\":\"prog_dio_7\",\"mode\":\"OUTPUT\"},{\"pinNumber\":8,\"type\":\"PROGRAMMABLE_DIO\",\"name\":\"prog_dio_8\",\"mqttTopic\":\"prog_dio_8\",\"mode\":\"OUTPUT\"},{\"pinNumber\":9,\"type\":\"PROGRAMMABLE_DIO\",\"name\":\"prog_dio_9\",\"mqttTopic\":\"prog_dio_9\",\"mode\":\"OUTPUT\"},{\"pinNumber\":10,\"type\":\"PROGRAMMABLE_DIO\",\"name\":\"prog_dio_10\",\"mqttTopic\":\"prog_dio_10\",\"mode\":\"OUTPUT\"},{\"pinNumber\":11,\"type\":\"PROGRAMMABLE_DIO\",\"name\":\"prog_dio_11\",\"mqttTopic\":\"prog_dio_11\",\"mode\":\"OUTPUT\"},{\"pinNumber\":0,\"type\":\"ANALOG_OUTPUT_VOLTAGE\",\"name\":\"analog_out_0\",\"mqttTopic\":\"analog_out0\",\"mode\":\"OUTPUT\",\"minValue\":0.0,\"maxValue\":1000.0,\"scaleFactor\":1.0,\"offset\":0.0,\"precision\":0,\"unit\":\"Lux\",\"lookupTable\":[{\"input\":0,\"output\":0},{\"input\":10,\"output\":1000}]},{\"pinNumber\":1,\"type\":\"ANALOG_OUTPUT_VOLTAGE\",\"name\":\"analog_out_1\",\"mqttTopic\":\"analog_out1\",\"mode\":\"OUTPUT\",\"minValue\":0.0,\"maxValue\":10.0,\"scaleFactor\":1.0,\"offset\":0.0,\"unit\":\"Lux\",\"lookupTable\":[{\"input\":0,\"output\":0},{\"input\":10,\"output\":1000}]},{\"pinNumber\":2,\"type\":\"ANALOG_OUTPUT_VOLTAGE\",\"name\":\"analog_out_2\",\"mqttTopic\":\"analog_out2\",\"mode\":\"OUTPUT\",\"minValue\":0.0,\"maxValue\":10.0,\"scaleFactor\":1.0,\"offset\":0.0},{\"pinNumber\":3,\"type\":\"ANALOG_OUTPUT_VOLTAGE\",\"name\":\"analog_out_3\",\"mqttTopic\":\"analog_out3\",\"mode\":\"OUTPUT\",\"minValue\":0.0,\"maxValue\":10.0,\"scaleFactor\":1.0,\"offset\":0.0}]}";

ConfigManager &ConfigManager::getInstance()
{
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadDefaultConfig()
{
    Serial.println("Loading default embedded config...");

    // Read JSON from PROGMEM into a buffer first
    char configBuffer[5120];

    strcpy_P(configBuffer, (const char *)DEFAULT_CONFIG);

    // Parse the embedded config
    ArduinoJson::DynamicJsonDocument doc(16384);
    // Cast to const char* to force ArduinoJson to make a copy of the strings
    // since configBuffer is on the stack and will be destroyed
    ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, (const char *)configBuffer);

    if (error)
    {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return false;
    }

    configDoc_ = doc;
    return parseJsonConfig(doc);
}

bool ConfigManager::loadFromStorage()
{
    // Use Partition 4 (User Data) - same as Logger and NetworkSettings
    InternalStorage storage(3, "user", FS_LITTLEFS);

    if (!storage.begin())
    {
        Serial.println("Cannot access user partition for config");
        Serial.println("Loading default config instead");
        return loadDefaultConfig();
    }

    // Try to open config file
    auto root = storage.getRootFolder();
    auto file = root.createFile("config.json", FileMode::READ);

    if (!file.exists())
    {
        Serial.println("No config.json found in flash storage");
        Serial.println("Loading default config instead");
        return loadDefaultConfig();
    }

    // Read file content
    size_t fileSize = file.available();
    if (fileSize == 0 || fileSize > 16384)
    { // Max 16KB for config
        Serial.println("Invalid config file size");
        file.close();
        return loadDefaultConfig();
    }

    uint8_t *buffer = new uint8_t[fileSize + 1];
    size_t bytesRead = file.read(buffer, fileSize);
    buffer[bytesRead] = '\0';
    file.close();

    // Parse JSON
    ArduinoJson::DynamicJsonDocument doc(16384);
    // Cast to const char* to force ArduinoJson to make a copy of the strings
    // since we delete the buffer immediately after
    ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, (const char *)buffer);
    delete[] buffer;

    if (error)
    {
        Serial.print("Config parse error: ");
        Serial.println(error.c_str());
        Serial.println("Loading default config instead");
        return loadDefaultConfig();
    }

    if (!parseJsonConfig(doc))
    {
        Serial.println("Config validation failed");
        Serial.println("Loading default config instead");
        return loadDefaultConfig();
    }

    configDoc_ = doc;

    // Check if embedded config has a newer version
    uint32_t currentVersion = getConfigVersion();
    uint32_t embeddedVersion = getEmbeddedVersion();

    Serial.print("Config loaded from QSPI flash (version ");
    Serial.print(currentVersion);
    Serial.println(")");

    if (embeddedVersion > currentVersion)
    {
        Serial.print("⚠ Embedded config version ");
        Serial.print(embeddedVersion);
        Serial.print(" is newer than QSPI version ");
        Serial.println(currentVersion);
        Serial.println("Auto-migrating to newer configuration...");

        if (loadDefaultConfig() && saveToStorage())
        {
            Serial.println("✓ Configuration upgraded successfully");
        }
        else
        {
            Serial.println("✗ Failed to upgrade configuration, continuing with current");
        }
    }

    return true;
}

bool ConfigManager::loadFromJson(const String &jsonString)
{
    Serial.println("Loading config from JSON...");

    ArduinoJson::DynamicJsonDocument doc(16384);
    ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, jsonString);

    if (error)
    {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return false;
    }

    if (!parseJsonConfig(doc))
    {
        Serial.println("Config validation failed, reverting to previous");
        return false;
    }

    configDoc_ = doc;
    Serial.println("Config updated successfully");
    return saveToStorage();
}

bool ConfigManager::saveToStorage()
{
    // Use Partition 4 (User Data)
    InternalStorage storage(3, "user", FS_LITTLEFS);

    if (!storage.begin())
    {
        Serial.println("Cannot access user partition for config");

        // Try to format if needed
        if (storage.format(FS_LITTLEFS) && storage.begin())
        {
            Serial.println("User partition formatted successfully");
        }
        else
        {
            Serial.println("Failed to save config to storage");
            return false;
        }
    }

    // Serialize config to JSON
    String json = toJson();

    // Write to file
    auto root = storage.getRootFolder();
    auto file = root.createFile("config.json", FileMode::WRITE);

    if (!file.exists())
    {
        Serial.println("Failed to create config.json");
        return false;
    }

    size_t bytesWritten = file.write((const uint8_t *)json.c_str(), json.length());
    file.close();

    if (bytesWritten == json.length())
    {
        Serial.print("Config saved to QSPI flash (");
        Serial.print(bytesWritten);
        Serial.println(" bytes)");
        return true;
    }
    else
    {
        Serial.println("Failed to write all config data");
        return false;
    }
}

bool ConfigManager::factoryReset()
{
    Serial.println("=== Factory Reset ===");
    Serial.println("Resetting to embedded default configuration...");

    if (!loadDefaultConfig())
    {
        Serial.println("Failed to load default config");
        return false;
    }

    if (!saveToStorage())
    {
        Serial.println("Failed to save default config to QSPI");
        return false;
    }

    Serial.println("Factory reset complete!");
    Serial.print("Configuration restored to version ");
    Serial.println(getConfigVersion());
    return true;
}

uint32_t ConfigManager::getConfigVersion() const
{
    return configDoc_["version"] | 0;
}

uint32_t ConfigManager::getEmbeddedVersion() const
{
    // Parse embedded config to get version
    ArduinoJson::DynamicJsonDocument doc(6144);

    // Read JSON from PROGMEM
    char configBuffer[5120];
    strcpy_P(configBuffer, (const char *)DEFAULT_CONFIG);

    ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, configBuffer);

    if (error)
    {
        Serial.print("Error reading embedded config version: ");
        Serial.println(error.c_str());
        return 0;
    }

    return doc["version"] | 0;
}

const std::vector<IO::PinConfiguration> &ConfigManager::getPins() const
{
    return pins_;
}

String ConfigManager::getDeviceName() const
{
    return String(deviceName_.c_str());
}

String ConfigManager::toJson() const
{
    String output;
    ArduinoJson::serializeJson(configDoc_, output);
    return output;
}

bool ConfigManager::validate() const
{
    return !pins_.empty();
}

bool ConfigManager::parseJsonConfig(ArduinoJson::JsonDocument &doc)
{
    // Load device name
    if (doc.containsKey("deviceName"))
    {
        deviceName_ = doc["deviceName"].as<std::string>();
    }
    else
    {
        deviceName_ = "Portenta Machine Control";
        doc["deviceName"] = deviceName_; // Inject into document
    }

    pins_.clear();

    // Check if pins exists
    if (!doc.containsKey("pins"))
    {
        Serial.println("ERROR: No 'pins' key in config");
        return false;
    }

    // Validate pins array is not empty
    if (doc["pins"].size() == 0)
    {
        Serial.println("ERROR: 'pins' array is empty");
        return false;
    }

    // Iterate through pins array directly
    for (size_t i = 0; i < doc["pins"].size(); i++)
    {
        auto pin = doc["pins"][i];

        // Validate mandatory fields
        if (pin["type"].isNull() || pin["name"].isNull())
        {
            Serial.print("Skipping invalid pin configuration at index ");
            Serial.println(i);
            continue;
        }

        IO::PinConfiguration cfg;

        cfg.pinNumber = pin["pinNumber"] | 0;
        cfg.type = stringToPinType(pin["type"].as<String>());
        cfg.mode = stringToPinMode(pin["mode"].as<String>());
        cfg.name = pin["name"].as<std::string>();
        cfg.mqttTopic = pin["mqttTopic"].as<std::string>();
        cfg.sampleRateMs = pin["sampleRateMs"] | 100;
        cfg.scaleFactor = pin["scaleFactor"] | 1.0f;
        cfg.offset = pin["offset"] | 0.0f;
        cfg.debounceMs = pin["debounceMs"] | 50;
        cfg.minValue = pin["minValue"] | 0.0f;
        cfg.maxValue = pin["maxValue"] | 0.0f;
        cfg.invertLogic = pin["invertLogic"] | false;
        cfg.precision = pin["precision"] | 2;
        cfg.unit = pin["unit"] | "";

        if (pin.containsKey("lookupTable"))
        {
            ArduinoJson::JsonArray table = pin["lookupTable"];
            for (ArduinoJson::JsonObject point : table)
            {
                if (point.containsKey("input") && point.containsKey("output"))
                {
                    cfg.lookupTable.push_back(std::make_pair(point["input"].as<float>(), point["output"].as<float>()));
                }
            }
        }

        pins_.push_back(cfg);
    }

    Serial.print("Loaded ");
    Serial.print(pins_.size());
    Serial.println(" pins from config");
    return true;
}

IO::PinType ConfigManager::stringToPinType(const String &typeStr)
{
    if (typeStr == "DIGITAL_OUTPUT")
        return IO::PinType::DIGITAL_OUTPUT;
    if (typeStr == "DIGITAL_INPUT")
        return IO::PinType::DIGITAL_INPUT;
    if (typeStr == "PROGRAMMABLE_DIO")
        return IO::PinType::PROGRAMMABLE_DIO;
    if (typeStr == "ANALOG_INPUT_VOLTAGE")
        return IO::PinType::ANALOG_INPUT_VOLTAGE;
    if (typeStr == "ANALOG_INPUT_CURRENT")
        return IO::PinType::ANALOG_INPUT_CURRENT;
    if (typeStr == "ANALOG_OUTPUT_VOLTAGE")
        return IO::PinType::ANALOG_OUTPUT_VOLTAGE;
    if (typeStr == "ANALOG_OUTPUT_CURRENT")
        return IO::PinType::ANALOG_OUTPUT_CURRENT;
    if (typeStr == "RTD_INPUT")
        return IO::PinType::RTD_INPUT;
    if (typeStr == "THERMOCOUPLE_INPUT")
        return IO::PinType::THERMOCOUPLE_INPUT;
    if (typeStr == "ENCODER_INPUT")
        return IO::PinType::ENCODER_INPUT;

    Serial.print("Unknown pin type: ");
    Serial.println(typeStr);
    return IO::PinType::DIGITAL_OUTPUT;
}

IO::PinMode ConfigManager::stringToPinMode(const String &modeStr)
{
    if (modeStr == "INPUT")
        return IO::PinMode::INPUT;
    if (modeStr == "OUTPUT")
        return IO::PinMode::OUTPUT;
    if (modeStr == "INPUT_PULLUP")
        return IO::PinMode::INPUT_PULLUP;
    if (modeStr == "INPUT_PULLDOWN")
        return IO::PinMode::INPUT_PULLDOWN;

    return IO::PinMode::OUTPUT;
}
