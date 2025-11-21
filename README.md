# Arduino Portenta MQTT Controller

[![Build Status](https://github.com/maartenvanels/maartenvanels-portenta-mqtt-controller/actions/workflows/build.yml/badge.svg)](https://github.com/maartenvanels/maartenvanels-portenta-mqtt-controller/actions/workflows/build.yml)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Framework-orange)
![Language](https://img.shields.io/badge/Language-C%2B%2B-blue)
![Status](https://img.shields.io/badge/Status-Active-green)

A robust MQTT-enabled controller for Arduino Portenta Machine Control with comprehensive I/O management via MQTT.

## Features

- **Network Management**

  - WiFi support with automatic reconnection
  - Ethernet support (experimental, see [ETHERNET_TROUBLESHOOTING.md](docs/ETHERNET_TROUBLESHOOTING.md))
  - Configurable via [include/credentials.h](include/credentials.h)

- **MQTT Integration**

  - Full MQTT client implementation
  - Configurable topics for each I/O pin
  - Last Will Testament (LWT) for connection status
  - Automatic reconnection with health monitoring
  - Periodic state publishing

- **I/O Support**

  - Digital inputs (8 channels, 24V)
  - Digital outputs (8 relays)
  - Analog voltage inputs (3 channels, 0-10V)
  - Analog current inputs (3 channels, 4-20mA)
  - Analog voltage outputs (4 channels, 0-10V)
  - Programmable Digital I/O (12 channels)
  - RTD temperature inputs (3 channels)
  - Configurable debouncing and sampling rates

- **Configuration**
  - JSON-based configuration file system
  - Per-pin MQTT topic configuration
  - Runtime reconfiguration via MQTT
  - See [PROGRAMMABLE_DIO_USAGE.md](docs/PROGRAMMABLE_DIO_USAGE.md) for examples

## Project Structure

```
portenta-mqtt-controller/
├── docs/                         # Documentation
│   ├── PROGRAMMABLE_DIO_USAGE.md    # Guide for configuring I/O pins
│   ├── RUNTIME_CONFIGURATION.md     # Hot-reload & MQTT config guide
│   ├── OTA.md                       # Over-The-Air update guide
│   ├── QSPI_FLASH_SETUP.md          # Flash memory setup (Required!)
│   ├── HOME_ASSISTANT_INTEGRATION.md # Home Assistant discovery setup
│   ├── ETHERNET_TROUBLESHOOTING.md  # Ethernet connectivity guide
│   └── ARCHITECTURE.md               # Technical architecture
├── src/                          # Source code
│   ├── main.cpp                     # Main application
│   ├── hardware/                    # Hardware abstraction layer
│   ├── io/                          # I/O pin handlers
│   └── config/                      # Configuration management
├── include/                      # Header files
│   ├── credentials.h.example        # Template for WiFi/MQTT credentials
│   └── io/                          # I/O controller headers
├── dotnet-mqtt-client/           # .NET MQTT client (example/testing)
├── data/                         # Configuration files (config.json)
└── platformio.ini                # PlatformIO build configuration
```

## Quick Start

### Prerequisites

- [PlatformIO](https://platformio.org/) installed (CLI or IDE)
- Arduino Portenta Machine Control board
- MQTT broker (e.g., Mosquitto, HiveMQ)

### Setup

1. **Clone the repository:**

```bash
git clone <repository-url>
cd portenta-mqtt-controller
```

2. **Configure credentials:**

```bash
# Copy the example credentials file
cp include/credentials.h.example include/credentials.h

# Edit credentials.h with your WiFi and MQTT settings
```

3. **Build and upload:**

```bash
# Build the firmware
pio run

# Upload to Portenta
pio run --target upload

# Monitor serial output
pio device monitor -b 115200
```

### Configuration File

Create a `data/config.json` file to configure I/O pins. See [PROGRAMMABLE_DIO_USAGE.md](docs/PROGRAMMABLE_DIO_USAGE.md) for examples.

Example minimal configuration:

```json
{
  "pins": [
    {
      "type": "digital_input",
      "pin": 0,
      "name": "door_sensor",
      "mode": "input",
      "mqtt_topic": "portenta/door/state"
    }
  ]
}
```

## MQTT Topics

Default topic structure (configurable per pin in config.json):

**System Status:**

- `portenta/status` - Online/offline status (LWT)
- `portenta/status/detail` - Detailed system status (JSON with uptime, health, network info)

**Input Pins (published on state change):**

- `portenta/digital_in{0-7}/state` - Digital input states
- `portenta/analog_in{0-2}/state` - Analog voltage input (0-10V)
- `portenta/analog_current{0-2}/state` - Analog current input (4-20mA)
- `portenta/temp_probe{0-2}/state` - RTD temperature readings
- `portenta/prog_dio/{name}/state` - Programmable DIO configured as input

**Output Pins (subscribe to control):**

- `portenta/relay{0-7}/set` - Control digital outputs (relays)
- `portenta/analog_out{0-3}/set` - Set analog voltage outputs (0-10V)
- `portenta/prog_dio/{name}/set` - Control programmable DIO outputs

**Configuration:**

- `portenta/config/reload` - Trigger configuration reload
- `portenta/pins/list` - Request list of all configured pins

See [PROGRAMMABLE_DIO_USAGE.md](docs/PROGRAMMABLE_DIO_USAGE.md) for detailed MQTT examples.

## .NET MQTT Client

A companion .NET console application is included for monitoring and controlling the Portenta:

```bash
cd dotnet-mqtt-client/PortentaMqttClient
dotnet run
```

See [dotnet-mqtt-client/README.md](dotnet-mqtt-client/README.md) for documentation.

## Development

### Code Style

- C++17 standard
- Object-oriented design with HAL (Hardware Abstraction Layer)
- Comprehensive error handling
- See [ARCHITECTURE.md](docs/ARCHITECTURE.md) for design details

### Building from Source

```bash
# Clean build
pio run --target clean

# Build for specific environment
pio run -e portenta_h7_m7

# Verbose build output
pio run -v
```

## Troubleshooting

### Common Issues

1. **WiFi connection fails**

   - Verify credentials in `include/credentials.h`
   - Check WiFi network is 2.4GHz (Portenta doesn't support 5GHz)
   - Monitor serial output for connection status

2. **MQTT connection fails**

   - Verify broker address, port, and credentials
   - Check firewall settings
   - Ensure MQTT_MAX_PACKET_SIZE is large enough (set in platformio.ini)
   - See serial output for connection error codes

3. **Ethernet issues**

   - See [ETHERNET_TROUBLESHOOTING.md](docs/ETHERNET_TROUBLESHOOTING.md)
   - Note: RTC is disabled when using Ethernet (Wire1 conflict)

4. **I/O not working**

   - Check power supply to Machine Control (24V)
   - Verify pin configuration in `data/config.json`
   - Monitor serial output for hardware initialization errors

5. **Build fails**
   - Update PlatformIO: `pio upgrade`
   - Clean build directory: `pio run --target clean`
   - Check PlatformIO dependencies are installed

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of conduct, and the process for submitting pull requests.

### Getting Started

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request
