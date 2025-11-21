# Programmable DIO Usage Guide

## Overview

The Arduino Portenta H7 Machine Control has 12 programmable digital I/O channels configurable as input or output:

- **Channels 0-5**: Configured as INPUTS
- **Channels 6-11**: Configured as OUTPUTS

## Hardware Specifications

- **Voltage level**: 24V tolerant
- **Bidirectional pins**: Each pin can read and write
- **Sampling rate**: Inputs checked every 100ms
- **State change detection**: MQTT messages sent only on change

## MQTT Topics

### Configuration Topics
Programmable DIOs use flexible, user-defined topics configured in `data/config.json`.

**Default pattern:**
- **Inputs**: `portenta/prog_dio/{name}/state`
- **Outputs**: `portenta/prog_dio/{name}/set`

Example configuration:
```json
{
  "type": "programmable_dio",
  "pin": 0,
  "name": "door_sensor",
  "mode": "input",
  "mqtt_topic": "portenta/prog_dio/door_sensor/state"
}
```

## Usage

### 1. Controlling Outputs

**Via mosquitto_pub:**
```bash
# Turn output ON
mosquitto_pub -h <broker_ip> -t "portenta/prog_dio/output1/set" -m "1"

# Turn output OFF
mosquitto_pub -h <broker_ip> -t "portenta/prog_dio/output1/set" -m "0"
```

**Via Python:**
```python
import paho.mqtt.client as mqtt
import json

client = mqtt.Client()
client.connect("broker_ip", 1883, 60)

# Turn output ON
client.publish("portenta/prog_dio/output1/set", "1")

# Turn output OFF
client.publish("portenta/prog_dio/output1/set", "0")

client.disconnect()
```

### 2. Monitoring Inputs

**Subscribe to specific input:**
```bash
mosquitto_sub -h <broker_ip> -t "portenta/prog_dio/input1/state"
```

**Subscribe to all programmable DIOs:**
```bash
mosquitto_sub -h <broker_ip> -t "portenta/prog_dio/#"
```

**Python example:**
```python
import paho.mqtt.client as mqtt

def on_message(client, userdata, msg):
    print(f"Topic: {msg.topic}, Payload: {msg.payload.decode()}")

client = mqtt.Client()
client.on_message = on_message
client.connect("broker_ip", 1883, 60)

client.subscribe("portenta/prog_dio/+/state")
client.loop_forever()
```

## Payload Format

### State Messages (from Portenta)
```json
{
  "value": 1.0,
  "timestamp": 123456
}
```

**Fields:**
- `value` (number): 0.0 for LOW, 1.0 for HIGH
- `timestamp` (number): Milliseconds since boot

### Command Messages (to Portenta)
Simple value: `"1"` or `"0"`

Or JSON format:
```json
{
  "value": 1
}
```

## Configuration

Edit `data/config.json` to configure pins:

```json
{
  "pins": [
    {
      "type": "programmable_dio",
      "pin": 0,
      "name": "door_sensor",
      "mode": "input",
      "mqtt_topic": "portenta/prog_dio/door_sensor/state",
      "sampleRateMs": 100
    },
    {
      "type": "programmable_dio",
      "pin": 6,
      "name": "conveyor_motor",
      "mode": "output",
      "mqtt_topic": "portenta/prog_dio/conveyor_motor/set"
    }
  ]
}
```

**Configuration options:**
- `pin`: Hardware channel (0-11)
- `name`: Unique identifier
- `mode`: `"input"` or `"output"`
- `mqtt_topic`: MQTT topic for this pin
- `sampleRateMs`: Sampling interval for inputs (default: 100ms)
- `debounceMs`: Debounce time for inputs (optional, default: 0ms)

## Testing

### Basic Output Test
```bash
# Terminal 1: Monitor state
mosquitto_sub -h <broker_ip> -t "portenta/prog_dio/#" -v

# Terminal 2: Send commands
mosquitto_pub -h <broker_ip> -t "portenta/prog_dio/output1/set" -m "1"
sleep 1
mosquitto_pub -h <broker_ip> -t "portenta/prog_dio/output1/set" -m "0"
```

### Input Monitoring Test
1. Subscribe to input topics: `mosquitto_sub -h <broker_ip> -t "portenta/prog_dio/+/state"`
2. Apply 24V signal to input channel (0-5)
3. Verify state change messages appear

## Troubleshooting

### Output not responding
- Verify pin name matches configuration
- Check MQTT connection: `mosquitto_sub -h <broker_ip> -t "portenta/#"`
- Monitor serial output for errors
- Ensure output isn't already in desired state

### Input not publishing
- Verify 24V signal is present
- Check pin is configured in config.json
- Verify `sampleRateMs` isn't too high (default: 100ms)
- Check serial monitor for initialization messages

### Wrong pin number
In config.json:
- **Inputs**: pin 0-5 (hardware channels 0-5)
- **Outputs**: pin 6-11 (hardware channels 6-11)

This mapping matches the Arduino MachineControl library.

## Advanced Configuration

### Custom Sample Rate
Lower sample rate = faster detection but higher CPU/MQTT load:
```json
{
  "sampleRateMs": 50  // 50ms for faster detection
}
```

### Debouncing
Add debouncing for noisy inputs:
```json
{
  "debounceMs": 50  // Ignore changes within 50ms
}
```

### Descriptive Names
Use meaningful names for clarity:
```json
{
  "name": "factory_door_sensor",
  "mqtt_topic": "factory/door/sensor/state"
}
```

## Code References

- Handler: [ProgrammableDIOHandler.cpp](../src/io/ProgrammableDIOHandler.cpp)
- HAL: [MachineControlHAL.cpp](../src/hardware/MachineControlHAL.cpp)
- Configuration: [config.json](../data/config.json)
