# Development: Testing Strategy

## Overzicht

Dit document beschrijft strategieën om code te testen zonder afhankelijk te zijn van fysieke hardware. Dit is bedoeld voor ontwikkelaars die bijdragen aan het project.

## Test Architectuur

### 1. Native Unit Testing (Aanbevolen Start)

**Voordelen:**
- Snelle feedback loop
- Geen hardware nodig
- CI/CD friendly
- Debuggen met standaard tools

**Implementatie met PlatformIO:**

```ini
; platformio.ini - voeg toe aan bestaande configuratie

[env:native]
platform = native
test_framework = unity
build_flags =
    -std=c++14
    -D UNIT_TEST
    -I include/
    -I src/
lib_deps =
    bblanchon/ArduinoJson @ ^6.21.3
test_build_src = yes
```

**Voorbeeld test structuur:**

```
test/
├── native/                      # Tests die op PC draaien
│   ├── test_config_manager/
│   │   └── test_main.cpp
│   ├── test_io_controller/
│   │   └── test_main.cpp
│   └── mocks/                   # Mock Arduino libraries
│       ├── Arduino.h
│       ├── PubSubClient.h
│       └── MachineControl.h
├── embedded/                    # Tests op hardware
│   └── test_integration/
└── README.md
```

### 2. Hardware Abstraction Layer (HAL) Verbeteren

De bestaande `MachineControlHAL` moet uitgebreid worden met een interface voor testing:

```cpp
// include/hardware/IHardwareInterface.h
class IHardwareInterface {
public:
    virtual bool digitalRead(uint8_t pin) = 0;
    virtual void digitalWrite(uint8_t pin, bool value) = 0;
    virtual float analogRead(uint8_t channel) = 0;
    virtual void analogWrite(uint8_t channel, float voltage) = 0;
    // ... meer methodes
};

// Voor productie
class MachineControlHAL : public IHardwareInterface { /*...*/ };

// Voor testing
class MockHardware : public IHardwareInterface {
    std::map<uint8_t, bool> digitalStates;
    std::map<uint8_t, float> analogStates;

public:
    bool digitalRead(uint8_t pin) override {
        return digitalStates[pin];
    }

    void setDigitalState(uint8_t pin, bool state) {
        digitalStates[pin] = state;
    }
    // ... implementatie voor testing
};
```

### 3. Emulatie Opties

#### Option A: Renode (Meest geschikt voor dit project)

[Renode](https://renode.io/) is een open-source emulator die ARM Cortex-M7 ondersteunt.

**Setup:**

```bash
# Download Renode
wget https://github.com/renode/renode/releases/download/v1.14.0/renode-1.14.0.linux-portable.tar.gz
tar -xf renode-1.14.0.linux-portable.tar.gz

# Start Renode
./renode/renode
```

**Renode script voor Portenta H7:**

```robot
# portenta_h7.resc
using sysbus

mach create "Portenta H7"
machine LoadPlatformDescription @platforms/cpus/stm32h747.repl

# Load your firmware
sysbus LoadELF @.pio/build/portenta_h7_m7/firmware.elf

# Setup UART
showAnalyzer sysbus.usart1

# Start emulation
start
```

**Beperkingen:**
- Specifieke Portenta peripherals niet volledig gesimuleerd
- Machine Control I/O niet out-of-the-box beschikbaar
- Vereist custom peripheral models

#### Option B: QEMU

QEMU ondersteunt ARM Cortex-M, maar vereist een generieke board definitie:

```bash
# Build firmware voor QEMU
pio run -e qemu_test

# Run in QEMU
qemu-system-arm -M netduinoplus2 \
    -kernel .pio/build/qemu_test/firmware.elf \
    -serial stdio \
    -nographic
```

### 4. Simulator-in-the-Loop Testing

Maak een PC-gebaseerde simulator die de Portenta gedrag nabootst:

```
┌─────────────────────────────────────┐
│  PC Simulator (Python/C++)          │
│                                     │
│  ┌─────────────────────────────┐   │
│  │  Mock MQTT Broker           │   │
│  └─────────────────────────────┘   │
│                                     │
│  ┌─────────────────────────────┐   │
│  │  Virtual I/O Simulator      │   │
│  │  - Digital inputs/outputs   │   │
│  │  - Analog simulation        │   │
│  └─────────────────────────────┘   │
│                                     │
│  ┌─────────────────────────────┐   │
│  │  Network Stack Mock         │   │
│  └─────────────────────────────┘   │
└─────────────────────────────────────┘
```

**Python MQTT Simulator voorbeeld:**

```python
# test/simulator/mqtt_simulator.py
import paho.mqtt.client as mqtt
import json
import time
from dataclasses import dataclass

@dataclass
class VirtualPin:
    type: str
    pin: int
    state: any
    topic: str

class PortentaSimulator:
    def __init__(self, mqtt_broker="localhost"):
        self.client = mqtt.Client("portenta_simulator")
        self.client.on_message = self.on_message
        self.pins = {}

    def add_digital_input(self, pin, topic):
        self.pins[pin] = VirtualPin("digital_in", pin, False, topic)

    def simulate_input_change(self, pin, state):
        """Simuleer een input verandering"""
        if pin in self.pins:
            self.pins[pin].state = state
            self.client.publish(
                self.pins[pin].topic,
                json.dumps({"state": state})
            )

    def on_message(self, client, userdata, msg):
        """Ontvang commands van de portenta"""
        print(f"Received: {msg.topic} - {msg.payload}")

    def run_test_scenario(self):
        """Draai een test scenario"""
        # Test 1: Simuleer een deur die open gaat
        print("Test: Door opens")
        self.simulate_input_change(0, True)
        time.sleep(1)

        # Test 2: Schakel relay aan
        print("Test: Turn on relay")
        self.client.publish("portenta/relay0/set", "1")
        time.sleep(1)
```

### 5. Continuous Integration Setup

**GitHub Actions workflow:**

```yaml
# .github/workflows/test.yml
name: Test

on: [push, pull_request]

jobs:
  native-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Set up Python
        uses: actions/setup-python@v2
        with:
          python-version: '3.x'

      - name: Install PlatformIO
        run: |
          pip install platformio

      - name: Run native tests
        run: pio test -e native

      - name: Build firmware
        run: pio run -e portenta_h7_m7

  mqtt-simulation-tests:
    runs-on: ubuntu-latest
    services:
      mosquitto:
        image: eclipse-mosquitto:latest
        ports:
          - 1883:1883
    steps:
      - uses: actions/checkout@v2

      - name: Run MQTT simulator tests
        run: |
          cd test/simulator
          python -m pytest test_mqtt_simulation.py
```

## Concrete Implementatie Stappen

### Fase 1: Unit Tests Setup (Week 1)

1. **Maak test directory structuur:**
```bash
mkdir -p test/native/{test_config_manager,test_io_controller,mocks}
```

2. **Implementeer Arduino mocks:**
```cpp
// test/native/mocks/Arduino.h
#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <stdint.h>
#include <string>

using String = std::string;

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1

unsigned long millis();
void delay(unsigned long ms);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);

#endif
```

3. **Eerste test schrijven:**
```cpp
// test/native/test_config_manager/test_main.cpp
#include <unity.h>
#include "ConfigManager.h"

void setUp(void) {
    // Set up before each test
}

void tearDown(void) {
    // Clean up after each test
}

void test_config_parse_valid_json() {
    const char* json = R"({
        "pins": [
            {"type": "digital_input", "pin": 0, "name": "test"}
        ]
    })";

    ConfigManager config;
    bool result = config.parseConfig(json);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, config.getPinCount());
}

void test_config_invalid_json() {
    ConfigManager config;
    bool result = config.parseConfig("invalid json");

    TEST_ASSERT_FALSE(result);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_config_parse_valid_json);
    RUN_TEST(test_config_invalid_json);
    return UNITY_END();
}
```

4. **Tests uitvoeren:**
```bash
pio test -e native
```





