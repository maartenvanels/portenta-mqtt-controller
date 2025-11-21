# Security Review & Recommendations

**Project:** Portenta Machine Control MQTT Controller
**Date:** 2025-10-26
**Status:** ✅ Safe for private network use
**Priority:** Medium (for production deployment)

---

## Executive Summary

The codebase has been reviewed for security vulnerabilities. **No critical security issues were found**. The application is safe for use in a trusted private network environment. However, several recommendations are provided for production deployment, especially if the system will be exposed to the internet or untrusted networks.

---

## Security Assessment

### ✅ Strong Security Practices

1. **Credentials Management**
   - WiFi and MQTT credentials stored in `include/credentials.h`
   - File is properly excluded from git via `.gitignore`
   - No hardcoded passwords in source code
   - ✅ **Status:** Compliant

2. **Input Validation**
   - Pin number validation in `IoController::validatePinNumber()`
   - Bounds checking: `if (channel >= kDigitalInputCount)`
   - Type safety via `PinType` enum
   - JSON parsing with error handling
   - ✅ **Status:** Compliant

3. **Memory Safety**
   - Extensive use of `std::unique_ptr` prevents memory leaks
   - No raw pointer ownership issues
   - Smart pointer usage throughout
   - ✅ **Status:** Compliant

4. **Buffer Overflow Protection**
   - Use of `std::string` and Arduino `String` classes
   - No unsafe C-style string operations
   - Fixed buffer size validation
   - ✅ **Status:** Compliant

5. **Code Architecture**
   - Singleton pattern prevents multiple instances
   - HAL abstraction layer for hardware isolation
   - Namespace organization (IO, HAL)
   - Const correctness throughout
   - ✅ **Status:** Compliant

---

## ⚠️ Recommendations for Production

### 1. MQTT Security Enhancements

**Current State:**
```cpp
// main.cpp:244
mqttClient.subscribe("portenta/+/set");    // Accepts all pin control topics
mqttClient.subscribe("portenta/config");   // Allows config override
```

**Risk:**
- Anyone with MQTT broker access can control all relays/outputs
- Configuration can be modified remotely without authentication beyond MQTT credentials
- No topic-level authorization

**Recommendation:**
- **Priority:** HIGH
- **Implementation:**

```cpp
// Option 1: MQTT ACLs (Recommended)
// Configure MQTT broker (e.g., Mosquitto) with ACL file:
// /etc/mosquitto/acl:
//   user portenta_controller
//   topic write portenta/+/state
//   topic read portenta/+/set
//   topic read portenta/config
//
//   user admin
//   topic readwrite #

// Option 2: Topic Whitelist in Code
const char* ALLOWED_SET_TOPICS[] = {
    "portenta/relay1/set",
    "portenta/relay2/set",
    "portenta/relay3/set",
    // ... etc
};

bool isTopicAllowed(const char* topic) {
    for (const char* allowed : ALLOWED_SET_TOPICS) {
        if (strcmp(topic, allowed) == 0) return true;
    }
    return false;
}

void handleMqttMessage(char *topic, byte *payload, unsigned int length) {
    if (!isTopicAllowed(topic)) {
        Serial.println("Unauthorized topic, ignoring");
        return;
    }
    // ... rest of handler
}
```

### 2. Add TLS/SSL Encryption for MQTT

**Current State:**
```cpp
WiFiClient wifiClient;  // Unencrypted connection
PubSubClient mqttClient(wifiClient);
```

**Risk:**
- MQTT traffic is transmitted in plaintext
- WiFi passwords and MQTT credentials visible to network sniffers
- Commands can be intercepted and replayed

**Recommendation:**
- **Priority:** HIGH (for production/internet exposure)
- **Implementation:**

```cpp
// In credentials.h, add:
// const char* MQTT_CA_CERT = "-----BEGIN CERTIFICATE-----\n...";

// In main.cpp:
#include <WiFiSSLClient.h>

WiFiSSLClient wifiClient;
PubSubClient mqttClient(wifiClient);

void setupMQTT() {
    // Configure SSL/TLS
    wifiClient.setCACert(MQTT_CA_CERT);

    // Use port 8883 for MQTT over TLS
    mqttClient.setServer(MQTT_BROKER, 8883);
    mqttClient.setCallback(handleMqttMessage);

    // ... rest of setup
}
```

### 3. Rate Limiting for MQTT Messages

**Current State:**
- No rate limiting on incoming MQTT messages
- System processes every message immediately

**Risk:**
- Flood attack possible via MQTT
- Could overwhelm system or cause relay chatter
- Resource exhaustion

**Recommendation:**
- **Priority:** MEDIUM
- **Implementation:**

```cpp
// Add to main.cpp globals:
uint32_t lastMqttMessage = 0;
const uint32_t MQTT_MIN_INTERVAL_MS = 100;  // 100ms = max 10 commands/sec
uint32_t droppedMessageCount = 0;

void handleMqttMessage(char *topic, byte *payload, unsigned int length) {
    // Rate limiting check
    uint32_t now = millis();
    if (now - lastMqttMessage < MQTT_MIN_INTERVAL_MS) {
        droppedMessageCount++;
        if (droppedMessageCount % 10 == 0) {
            Serial.print("WARNING: Rate limit exceeded, dropped ");
            Serial.print(droppedMessageCount);
            Serial.println(" messages");
        }
        return;
    }
    lastMqttMessage = now;

    // ... rest of handler
}
```

### 4. MQTT Message Size Validation

**Current State:**
```cpp
// platformio.ini
-D MQTT_MAX_PACKET_SIZE=1024

// No additional validation in handleMqttMessage
```

**Risk:**
- Large payloads could cause memory issues
- Config messages could be crafted to overflow buffers

**Recommendation:**
- **Priority:** MEDIUM
- **Implementation:**

```cpp
void handleMqttMessage(char *topic, byte *payload, unsigned int length) {
    // Validate message size
    const unsigned int MAX_CONFIG_SIZE = 8192;  // 8KB max for config
    const unsigned int MAX_COMMAND_SIZE = 32;   // 32 bytes for commands

    String topicStr = String(topic);

    if (topicStr == "portenta/config") {
        if (length > MAX_CONFIG_SIZE) {
            Serial.println("ERROR: Config message too large, ignoring");
            return;
        }
    } else if (length > MAX_COMMAND_SIZE) {
        Serial.println("ERROR: Command message too large, ignoring");
        return;
    }

    // ... rest of handler
}
```

### 5. Disable Debug Output in Production

**Current State:**
```cpp
Serial.println(message.c_str());  // Prints all MQTT messages
Serial.println(ssid);             // Prints WiFi SSID
```

**Risk:**
- Sensitive data visible in serial logs
- Could leak configuration details
- Performance overhead

**Recommendation:**
- **Priority:** LOW
- **Implementation:**

```cpp
// Add to platformio.ini for production builds:
// build_flags =
//     -D PRODUCTION_MODE

// In main.cpp:
#ifdef PRODUCTION_MODE
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
#else
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
#endif

// Replace Serial.println() calls:
DEBUG_PRINTLN("MQTT message received");
DEBUG_PRINT("Connecting to WiFi: ");
DEBUG_PRINTLN(ssid);
```

### 6. Configuration Change Authorization

**Current State:**
- Any MQTT message to `portenta/config` can update configuration
- No separate authorization required

**Risk:**
- Malicious config could disable safety features
- Could reassign critical pins
- Requires restart but still a risk

**Recommendation:**
- **Priority:** MEDIUM
- **Implementation:**

```cpp
// Option 1: Require authorization token
void handleMqttMessage(char *topic, byte *payload, unsigned int length) {
    String topicStr = String(topic);

    if (topicStr == "portenta/config") {
        // Parse JSON
        ArduinoJson::DynamicJsonDocument doc(4096);
        ArduinoJson::deserializeJson(doc, payload, length);

        // Check authorization token
        const char* token = doc["auth_token"];
        if (!token || strcmp(token, CONFIG_AUTH_TOKEN) != 0) {
            Serial.println("ERROR: Unauthorized config change attempt");
            return;
        }

        // ... proceed with config update
    }
}

// Option 2: Disable remote config updates entirely
// Comment out the subscribe line:
// mqttClient.subscribe("portenta/config");  // DISABLED for security
```

### 7. Watchdog Timer for System Recovery

**Current State:**
- No watchdog timer implemented
- System could hang without recovery

**Risk:**
- System lockup requires manual reset
- Loss of control in critical applications

**Recommendation:**
- **Priority:** LOW (reliability, not security)
- **Implementation:**

```cpp
#include <mbed.h>

void setup() {
    // ... existing setup

    // Enable watchdog (30 second timeout)
    mbed::Watchdog &watchdog = mbed::Watchdog::get_instance();
    watchdog.start(30000);  // 30 seconds
}

void loop() {
    // Feed the watchdog
    mbed::Watchdog::get_instance().kick();

    // ... rest of loop
}
```

---

## Implementation Priority

### Phase 1 - Critical (Before Internet Exposure)
1. ✅ MQTT TLS/SSL encryption
2. ✅ MQTT ACL configuration or topic whitelist
3. ✅ Message size validation

### Phase 2 - Important (Before Production)
4. ✅ Rate limiting
5. ✅ Configuration change authorization
6. ✅ Disable debug output

### Phase 3 - Optional (Hardening)
7. ✅ Watchdog timer
8. ✅ Audit logging
9. ✅ Intrusion detection

---

## Testing Recommendations

### Security Testing Checklist

- [ ] **MQTT Penetration Test**
  - [ ] Attempt unauthorized topic subscription
  - [ ] Send malformed JSON payloads
  - [ ] Test rate limiting with flood attack
  - [ ] Verify TLS certificate validation

- [ ] **Input Validation Test**
  - [ ] Send invalid pin numbers
  - [ ] Send out-of-range values
  - [ ] Test with extremely large payloads
  - [ ] Test special characters in strings

- [ ] **Network Security Test**
  - [ ] Verify credentials.h not in repository
  - [ ] Test behavior with wrong MQTT credentials
  - [ ] Verify encrypted MQTT traffic (Wireshark)
  - [ ] Test reconnection after network loss

- [ ] **Physical Security**
  - [ ] Secure physical access to device
  - [ ] Disable serial port in production enclosure
  - [ ] Document emergency shutdown procedure

---

## Compliance Considerations

### If deploying in regulated environments:

1. **IEC 62443** (Industrial Automation Security)
   - Network segmentation required
   - Access control lists mandatory
   - Audit logging recommended

2. **NIST Cybersecurity Framework**
   - Asset identification (document all I/O)
   - Risk assessment required
   - Incident response plan

3. **GDPR** (if processing personal data)
   - Not applicable unless logging user actions
   - If logging: implement data retention policy

---

## Deployment Checklist

### Before Production Deployment:

- [ ] Change default MQTT credentials in `credentials.h`
- [ ] Enable TLS/SSL for MQTT
- [ ] Configure MQTT broker ACLs
- [ ] Implement rate limiting
- [ ] Disable serial debug output
- [ ] Test emergency stop procedure
- [ ] Document all pin assignments
- [ ] Create backup of configuration
- [ ] Test rollback procedure
- [ ] Verify physical security of device
- [ ] Document incident response plan

---

## Contact & Maintenance

**Created by:** Claude Code AI Assistant
**Review Date:** 2025-10-26
**Next Review:** Before production deployment

**Questions or Updates:**
- Review this document before any network topology changes
- Update after implementing any of these recommendations
- Re-assess security when adding new features

---

## Appendix: Example Secure Configuration

### A. MQTT Broker Configuration (Mosquitto)

```conf
# /etc/mosquitto/mosquitto.conf

# Enable authentication
allow_anonymous false
password_file /etc/mosquitto/passwd

# Enable ACLs
acl_file /etc/mosquitto/acl

# Enable TLS
listener 8883
cafile /etc/mosquitto/ca_certificates/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate false

# Logging
log_type all
log_dest file /var/log/mosquitto/mosquitto.log
```

### B. MQTT ACL Configuration

```conf
# /etc/mosquitto/acl

# Admin user - full access
user admin
topic readwrite #

# Portenta device - limited access
user portenta_controller
topic write portenta/+/state
topic read portenta/+/set
topic read portenta/config

# Dashboard user - read-only
user dashboard
topic read portenta/#
```

### C. Creating MQTT Users

```bash
# Create password file
sudo mosquitto_passwd -c /etc/mosquitto/passwd admin
sudo mosquitto_passwd /etc/mosquitto/passwd portenta_controller
sudo mosquitto_passwd /etc/mosquitto/passwd dashboard

# Restart mosquitto
sudo systemctl restart mosquitto
```

---

**Document Version:** 1.0
**Status:** Draft for Implementation
