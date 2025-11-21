# Persistent Logging System

## Overzicht

De Portenta MQTT Controller heeft een persistent logging systeem dat logs opslaat in **QSPI Partition 4** (User Data). Dit betekent dat logs **bewaard blijven** zelfs na firmware updates of reboots!

```
┌─────────────────────────────────────┐
│ QSPI Flash Partition 4 (7 MB)      │
│                                     │
│ /user/system.log       ← Current   │
│ /user/system.log.old   ← Backup    │
│                                     │
│ Max 500 KB per file (default)      │
│ Auto-rotate when full               │
└─────────────────────────────────────┘
```

## Features

✅ **Persistent** - Logs blijven bestaan na firmware updates
✅ **Auto-rotating** - Automatische rotatie bij maximum grootte
✅ **Buffered writes** - Efficient QSPI gebruik
✅ **Multiple log levels** - DEBUG, INFO, WARNING, ERROR, CRITICAL
✅ **Timestamps** - Milliseconden sinds boot
✅ **Web API** - Logs lezen via HTTP
✅ **Filtering** - Lees logs sinds timestamp

## Quick Start

### 1. Initialiseer Logger in `setup()`

```cpp
#include "Logger.h"

void setup() {
    Serial.begin(115200);

    // Initialize logger (max 500 KB log file)
    Logging::Logger& logger = Logging::Logger::getInstance();
    logger.initialize(500);  // 500 KB max

    // Set minimum log level (optional, default is INFO)
    logger.setLogLevel(Logging::LogLevel::DEBUG);

    // Your other setup code...
}
```

### 2. Gebruik in Je Code

```cpp
#include "Logger.h"

void loop() {
    Logging::Logger& logger = Logging::Logger::getInstance();

    // Log with different levels
    logger.debug("Detailed debug information");
    logger.info("System started WiFi connection");
    logger.warning("MQTT reconnecting...");
    logger.error("Failed to read sensor!");
    logger.critical("SYSTEM CRASH IMMINENT!");

    // Or use convenient macros
    LOG_INFO("Temperature: " + String(temp) + "°C");
    LOG_ERROR("Connection timeout after " + String(retries) + " retries");
}
```

### 3. Lees Logs

**Via Web API:**
```bash
# Get last 100 log lines
curl http://192.168.1.100/api/logs

# Clear all logs
curl -X DELETE http://192.168.1.100/api/logs
```

**Via Serial Monitor:**
```cpp
String logs = logger.readLogs(50);  // Last 50 lines
Serial.println(logs);
```

## Log Levels

| Level | Value | Gebruik | Voorbeeld |
|-------|-------|---------|-----------|
| **DEBUG** | 0 | Gedetailleerde debug info | `"WiFi RSSI: -45 dBm"` |
| **INFO** | 1 | Normale operatie | `"MQTT connected"` |
| **WARNING** | 2 | Waarschuwingen | `"High CPU usage: 85%"` |
| **ERROR** | 3 | Fouten die recovery toelaten | `"Failed to read sensor, retrying..."` |
| **CRITICAL** | 4 | Kritieke fouten | `"Out of memory! Rebooting..."` |

**Filtering:**
```cpp
// Only log WARNING and above (WARNING, ERROR, CRITICAL)
logger.setLogLevel(Logging::LogLevel::WARNING);

// This will NOT be logged:
logger.debug("Debug msg");  // DEBUG < WARNING
logger.info("Info msg");     // INFO < WARNING

// These WILL be logged:
logger.warning("Warning!");   // WARNING >= WARNING
logger.error("Error!");       // ERROR >= WARNING
```

## Log Format

```
[TIMESTAMP][LEVEL] Message
```

**Voorbeeld:**
```
[12345][INFO] System boot completed
[15678][WARN] MQTT reconnecting...
[18901][ERROR] Sensor read failed: timeout
[20123][INFO] WiFi connected, IP: 192.168.1.100
```

- **TIMESTAMP**: Milliseconden sinds boot (`millis()`)
- **LEVEL**: DEBUG, INFO, WARN, ERROR, CRIT
- **Message**: Je bericht

## Auto-Rotation

Wanneer `system.log` de maximum grootte bereikt (default 500 KB):

```
Before:
/user/system.log         (500 KB - FULL!)
/user/system.log.old     (500 KB)

Rotation happens:
1. Delete system.log.old
2. Rename system.log -> system.log.old
3. Create new empty system.log

After:
/user/system.log         (0 KB - EMPTY)
/user/system.log.old     (500 KB - ROTATED)
```

**Totale log storage**: ~1 MB (2 files × 500 KB)

## Buffering & Performance

Logs worden **gebufferd** voor betere QSPI performance:

```cpp
// Automatic flush triggers:
1. Buffer full (10 entries)
2. 5 seconden passed
3. Manually: logger.flush()
```

**Voorbeeld:**
```cpp
for (int i = 0; i < 100; i++) {
    LOG_INFO("Loop iteration " + String(i));
    // Flushes happen automatically every 10 entries
}

// Manual flush before critical section:
logger.flush();  // Force write to QSPI now
rebootSystem();
```

## Web API

### GET /api/logs
Haal logs op (laatste 100 regels).

**Request:**
```http
GET /api/logs HTTP/1.1
Authorization: Bearer YOUR_TOKEN
```

**Response:**
```json
{
  "logs": "[12345][INFO] System boot...\n[15678][WARN] ...",
  "size": 245678,
  "count": 1543
}
```

### DELETE /api/logs
Wis alle logs.

**Request:**
```http
DELETE /api/logs HTTP/1.1
Authorization: Bearer YOUR_TOKEN
```

**Response:**
```json
{
  "success": true,
  "message": "Logs cleared"
}
```

## Code Examples

### 1. Log Systeem Events

```cpp
void connectToWiFi() {
    LOG_INFO("Connecting to WiFi: " + String(WIFI_SSID));

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;

        if (attempts % 5 == 0) {
            LOG_WARNING("WiFi connection attempt " + String(attempts) + "/20");
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        LOG_INFO("WiFi connected! IP: " + WiFi.localIP().toString());
    } else {
        LOG_ERROR("WiFi connection failed after 20 attempts");
    }
}
```

### 2. Log MQTT Messages

```cpp
void handleMqttMessage(char* topic, byte* payload, unsigned int length) {
    String message(reinterpret_cast<char*>(payload), length);

    LOG_DEBUG("MQTT message [" + String(topic) + "]: " + message);

    if (processMessage(topic, message)) {
        LOG_INFO("Processed MQTT command: " + String(topic));
    } else {
        LOG_ERROR("Failed to process MQTT message from topic: " + String(topic));
    }
}
```

### 3. Log Sensor Readings

```cpp
void readSensors() {
    float temp = readTemperature();

    if (isnan(temp)) {
        LOG_ERROR("Temperature sensor read failed!");
        return;
    }

    if (temp > 80.0) {
        LOG_CRITICAL("Temperature critical: " + String(temp) + "°C - SHUTDOWN!");
        emergencyShutdown();
    } else if (temp > 60.0) {
        LOG_WARNING("Temperature high: " + String(temp) + "°C");
    } else {
        LOG_DEBUG("Temperature normal: " + String(temp) + "°C");
    }
}
```

### 4. Log met Conditie

```cpp
void processData(const String& data) {
    static int errorCount = 0;

    if (!validateData(data)) {
        errorCount++;

        // Only log every 10th error to avoid spam
        if (errorCount % 10 == 0) {
            LOG_WARNING("Data validation failed " + String(errorCount) + " times");
        }

        if (errorCount > 100) {
            LOG_CRITICAL("Too many validation errors (" + String(errorCount) + ") - possible attack!");
        }
    }
}
```

### 5. Lees Logs Sinds Timestamp

```cpp
void checkForErrorsSince(uint32_t lastCheck) {
    Logging::Logger& logger = Logging::Logger::getInstance();

    String recentLogs = logger.readLogsSince(lastCheck);

    if (recentLogs.indexOf("[ERROR]") != -1 ||
        recentLogs.indexOf("[CRIT]") != -1) {

        Serial.println("ERRORS DETECTED IN RECENT LOGS:");
        Serial.println(recentLogs);

        // Send notification...
    }
}
```

## Best Practices

### ✅ DO

1. **Log belangrijke events**
   ```cpp
   LOG_INFO("System boot complete");
   LOG_INFO("MQTT connected to broker");
   ```

2. **Log errors met context**
   ```cpp
   LOG_ERROR("Sensor read failed: pin=" + String(pin) + " retries=" + String(retries));
   ```

3. **Gebruik juiste log levels**
   ```cpp
   LOG_DEBUG("Loop iteration 12345");        // Development only
   LOG_INFO("User logged in");               // Normal operation
   LOG_WARNING("Memory usage 85%");          // Potential issue
   LOG_ERROR("Failed to save config");       // Recoverable error
   LOG_CRITICAL("Out of memory - reboot!");  // System failure
   ```

4. **Flush voor critical operations**
   ```cpp
   LOG_CRITICAL("Entering bootloader...");
   logger.flush();  // Ensure log is written!
   enterBootloader();
   ```

### ❌ DON'T

1. **Log in high-frequency loops**
   ```cpp
   // BAD: 1000 logs per second!
   void loop() {
       LOG_DEBUG("Loop running");  // Every millisecond!
       delay(1);
   }

   // GOOD: Throttle logging
   void loop() {
       static uint32_t lastLog = 0;
       if (millis() - lastLog > 10000) {  // Every 10 seconds
           LOG_DEBUG("Loop heartbeat");
           lastLog = millis();
       }
   }
   ```

2. **Log sensitive data**
   ```cpp
   // BAD: Passwords in logs!
   LOG_INFO("Connecting with password: " + password);

   // GOOD: Redact sensitive info
   LOG_INFO("Connecting with password: ********");
   ```

3. **Log very long messages**
   ```cpp
   // BAD: 10KB JSON in single log
   LOG_DEBUG(hugejsonData);  // Slow!

   // GOOD: Summarize
   LOG_DEBUG("Received config: " + String(jsonData.length()) + " bytes");
   ```

## Troubleshooting

### Logs niet zichtbaar

**Probleem:** Logs worden niet naar QSPI geschreven

**Oplossing:**
```cpp
// Check if logger initialized
Logging::Logger& logger = Logging::Logger::getInstance();
if (!logger.initialize()) {
    Serial.println("Logger init failed!");
}

// Check log level
logger.setLogLevel(Logging::LogLevel::DEBUG);  // Log everything

// Manual flush
logger.flush();
```

### QSPI partition errors

**Symptoom:**
```
ERROR: Failed to format user partition for logs
```

**Oplossing:** Run QSPIFormat sketch (zie [QSPI_FLASH_SETUP.md](QSPI_FLASH_SETUP.md))

### Logs verdwenen

**Mogelijke oorzaken:**
1. Log rotatie gebeurde (check `system.log.old`)
2. Logs cleared via API
3. QSPI partition gewist

**Check:**
```cpp
size_t size = logger.getLogFileSize();
size_t count = logger.getLogCount();

Serial.print("Log file: ");
Serial.print(size);
Serial.print(" bytes, ");
Serial.print(count);
Serial.println(" lines");
```

## Performance Impact

**Memory:**
- Logger instance: ~200 bytes
- Buffer (10 entries): ~500 bytes
- Total: **< 1 KB RAM**

**QSPI Writes:**
- Buffered (10 entries or 5 seconds)
- ~10 writes/minute for typical usage
- QSPI has 100,000 erase cycles → **Years of operation**

**CPU:**
- Negligible (<0.1% CPU)
- No blocking operations
- Flush happens in background

## Log Analysis

### Via Serial Monitor

```cpp
void dumpLogs() {
    Logging::Logger& logger = Logging::Logger::getInstance();

    Serial.println("\n=== SYSTEM LOGS ===");
    String logs = logger.readLogs(100);
    Serial.println(logs);

    Serial.print("Total size: ");
    Serial.print(logger.getLogFileSize() / 1024);
    Serial.println(" KB");
}
```

### Via Web Interface

```javascript
// Fetch logs from web browser
fetch('/api/logs', {
    headers: {
        'Authorization': 'Bearer ' + token
    }
})
.then(res => res.json())
.then(data => {
    console.log('Logs:', data.logs);
    console.log('Size:', data.size, 'bytes');
    console.log('Count:', data.count, 'lines');
});
```

### Via MQTT (optioneel)

```cpp
void publishLogsToMQTT() {
    Logging::Logger& logger = Logging::Logger::getInstance();

    // Get recent errors
    uint32_t oneHourAgo = millis() - (60 * 60 * 1000);
    String recentLogs = logger.readLogsSince(oneHourAgo);

    // Publish to MQTT
    mqttClient.publish("portenta/logs/recent", recentLogs.c_str());
}
```

## Referenties

- [QSPI Flash Explained](QSPI_FLASH_EXPLAINED.md)
- [QSPI Flash Setup Guide](QSPI_FLASH_SETUP.md)
- [Arduino UnifiedStorage Docs](https://docs.arduino.cc/tutorials/portenta-h7/reading-writing-flash-memory)
