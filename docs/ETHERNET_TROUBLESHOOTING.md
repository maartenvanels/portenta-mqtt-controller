# Ethernet Troubleshooting - Portenta Machine Control

## Known Issues

The Arduino Portenta Machine Control has known hardware/driver issues with the Ethernet PHY that can cause:
- Periodic link loss
- MQTT connection errors (rc=-2)
- Flashing Ethernet LED behavior
- Ethernet interface hangs

## Symptoms

### MQTT Error -2 (Invalid Client ID)
```
18:29:20.402 > Connecting to MQTT broker: 192.168.18.74:1883
18:29:20.404 > MQTT connection failed, rc=-2
18:29:20.404 > Reason: -1=wrong protocol, -2=invalid client id, -3=unavailable, -4=bad credentials, -5=not authorized
```

**Note:** Error code `-2` is misleading! It often does NOT mean invalid client ID, but indicates:
- Ethernet link loss
- TCP connection failure
- Ethernet PHY hang

### Ethernet Link Loss
```
WARNING: Ethernet link lost! Attempting to restore...
ERROR: Ethernet link still down. Resetting PHY...
```

## Solutions

### 1. Automatic Ethernet Health Monitoring

The firmware checks Ethernet link status every 5 seconds and auto-restores:

**Location:** [src/main.cpp:262-317](../src/main.cpp#L262-L317)

```cpp
void checkEthernetHealth()
{
    // Check Ethernet link status every 5 seconds
    if (Ethernet.linkStatus() != LinkON) {
        // Attempt to restore connection
        Ethernet.maintain();

        // If still down, full PHY reset
        if (Ethernet.linkStatus() != LinkON) {
            // Reinitialize Ethernet
            // Restore MQTT connection
        }
    }
}
```

**Features:**
- ✅ Periodic link checks every 5 seconds
- ✅ Automatic `Ethernet.maintain()` calls
- ✅ PHY reset on persistent issues
- ✅ MQTT reconnect after recovery
- ✅ IP address preservation (DHCP or static)

### 2. MQTT Buffer Size Increased

MQTT buffer size increased from 256 to 1024 bytes to prevent connection issues:

**Location:** [platformio.ini:22](../platformio.ini#L22)

```ini
build_flags =
    -D MQTT_MAX_PACKET_SIZE=1024
```

### 3. MQTT Reconnect Delay

Prevents rapid reconnection attempts (5 second minimum between attempts):

**Location:** [src/main.cpp:345-348](../src/main.cpp#L345-L348)

```cpp
if (now - lastMqttAttempt < 5000) {  // Wait at least 5 seconds
    return;
}
```

### 4. Enhanced Debug Output

Extra debug info during MQTT connection attempts:

```
Connecting to MQTT broker: 192.168.18.74:1883
Client ID: portenta-mqtt-controller
Ethernet IP: 192.168.18.100
Connected to MQTT broker!
```

On failures:
```
MQTT connection failed, rc=-2
Ethernet link status: DOWN
```

## Manual Workarounds

### Option 1: Cable Replug
If Ethernet hangs:
1. Unplug cable
2. Wait 5 seconds
3. Replug cable
4. Firmware auto-detects and recovers

### Option 2: Device Reset
If everything hangs:
1. Press RESET button on Portenta
2. Device reboots
3. Ethernet reinitializes

### Option 3: Use WiFi
If Ethernet is too unreliable, switch to WiFi:

**Edit:** [include/credentials.h](../include/credentials.h)

```cpp
// Comment out to use WiFi instead
// #define USE_ETHERNET

// WiFi credentials
#define WIFI_SSID "your-ssid"
#define WIFI_PASSWORD "your-password"
```

WiFi is more stable on Portenta H7 than Ethernet.

## Monitoring

### Serial Monitor Output

View serial output for health status:

```bash
# PlatformIO
pio device monitor

# Arduino IDE
Tools > Serial Monitor (115200 baud)
```

### MQTT Last Will Testament (LWT)

Client automatically publishes "offline" to `portenta/status` on disconnect:

```bash
# Monitor status
mosquitto_sub -h 192.168.18.74 -t "portenta/status" -v

# Output:
portenta/status online   # Device connected
portenta/status offline  # Device disconnected (LWT triggered)
```

### Ethernet LED Behavior

| LED State | Meaning |
|-----------|---------|
| Solid | Link UP, connection stable |
| Fast flashing | Data transmission |
| Slow flashing | Link issues / PHY reset |
| Off | No link / cable disconnected |

## Best Practices

### 1. Cable Quality
- ✅ Use Cat5e or Cat6 cables
- ✅ Maximum length: 100 meters
- ❌ Avoid cheap/damaged cables
- ✅ Check connector quality (no loose pins)

### 2. Switch/Router Settings
- ✅ Disable auto-negotiation on switch port (force 100Mbps Full Duplex)
- ✅ Disable Energy Efficient Ethernet (EEE) / Green Ethernet
- ✅ Set static DHCP lease for Portenta
- ❌ Avoid PoE switches (Portenta needs external power)

### 3. Network Environment
- ✅ Use dedicated industrial switch if possible
- ✅ Keep Ethernet cable away from power cables
- ✅ Use shielded cable in noisy environments
- ❌ Avoid long runs in electrically noisy areas

### 4. Firmware Best Practices
- ✅ Monitor serial output for warnings
- ✅ Use MQTT LWT for disconnect detection
- ✅ Implement watchdog timer for system recovery
- ✅ Log Ethernet events for pattern analysis

## Error Codes Reference

| RC Code | Meaning | Likely Cause |
|---------|---------|--------------|
| -1 | Wrong protocol | MQTT broker version mismatch |
| -2 | Invalid client ID | **Ethernet link down** (most common) |
| -3 | Server unavailable | Broker offline or network issue |
| -4 | Bad credentials | Username/password incorrect |
| -5 | Not authorized | ACL restriction on broker |

**Note:** RC=-2 is misleading and usually indicates an Ethernet issue!

## Advanced Debugging

### Ethernet Library Patches

If issues persist, consider these patches:

1. **Increase PHY timeout:**
```cpp
// In setupEthernet()
for (int wait = 0; wait < 30; wait++) {  // Increase from 10 to 30
    delay(1000);
    if (Ethernet.linkStatus() == LinkON) break;
}
```

2. **Force static IP:**
```cpp
// Skip DHCP entirely
IPAddress ip(192, 168, 18, 100);
IPAddress dns(192, 168, 18, 1);
IPAddress gateway(192, 168, 18, 1);
IPAddress subnet(255, 255, 255, 0);
Ethernet.begin(ip, dns, gateway, subnet);
```

3. **Add hardware reset pin:**
```cpp
// If you wire up ETH_RST pin
pinMode(ETH_RST_PIN, OUTPUT);
digitalWrite(ETH_RST_PIN, LOW);
delay(100);
digitalWrite(ETH_RST_PIN, HIGH);
delay(500);
```

### Packet Capture

For detailed analysis:

```bash
# Capture MQTT traffic
tcpdump -i eth0 -w portenta.pcap host 192.168.18.100 and port 1883

# Analyze with Wireshark
wireshark portenta.pcap
```

Look for:
- TCP retransmissions
- Connection resets
- MQTT CONNECT/CONNACK packets

## Hardware Errata

Known hardware issues per Arduino forums:
- PHY chip (DP83848) sometimes has wake-up issues
- Board revision dependent
- Some boards have more issues than others

**Workaround:** Implemented health monitoring compensates for these issues.

## Support

If issues persist after these fixes:
1. Check [Arduino forum - Portenta H7](https://forum.arduino.cc/c/hardware/portenta/90)
2. Report issue on GitHub with:
   - Serial monitor output
   - Ethernet switch/router model
   - Cable length and type
   - Failure pattern (random vs periodic)

## Changelog

### v1.1 (2025-10-29)
- ✅ Added automatic Ethernet health monitoring
- ✅ Increased MQTT buffer size to 1024 bytes
- ✅ Enhanced debug output
- ✅ Link status verification before MQTT connect
- ✅ Auto PHY reset on link loss

### v1.0 (Earlier)
- Initial Ethernet support
- MQTT reconnect logic
- Basic error handling
