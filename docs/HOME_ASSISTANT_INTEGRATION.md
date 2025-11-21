# Home Assistant MQTT Discovery Integration

Deze Portenta MQTT Controller ondersteunt automatische integratie met Home Assistant via MQTT Discovery.

## Wat is MQTT Discovery?

MQTT Discovery is een Home Assistant feature waarmee apparaten zichzelf automatisch kunnen registreren. De Portenta publiceert configuratie berichten naar speciale topics, waarna alle I/O pins automatisch als entities in Home Assistant verschijnen.

## Vereisten

1. **Home Assistant** met MQTT integratie geconfigureerd
2. **MQTT Broker** (bijv. Mosquitto) toegankelijk voor beide devices
3. **MQTT Discovery enabled** in Home Assistant (standaard aan)

## Configuratie in Home Assistant

### MQTT Broker Setup

Zorg dat MQTT Discovery enabled is in je `configuration.yaml`:

```yaml
mqtt:
  broker: 192.168.1.100
  discovery: true
  discovery_prefix: homeassistant
```

> **Note**: De Portenta gebruikt standaard het prefix `homeassistant`. Als je een ander prefix gebruikt, moet je dit aanpassen in `HADiscovery.cpp`.

## Automatische Entity Discovery

### Bij Opstarten

Wanneer de Portenta opstart en verbinding maakt met de MQTT broker:

1. Alle geconfigureerde pins worden gepubliceerd naar discovery topics
2. Home Assistant detecteert automatisch de nieuwe entities
3. Entities worden toegevoegd aan het device "Portenta Machine Control"

### Supported Entity Types

De Portenta publiceert verschillende entity types afhankelijk van de pin configuratie:

| Pin Type | Home Assistant Entity | Beschrijving |
|----------|----------------------|--------------|
| **DIGITAL_OUTPUT** | `switch` | Relay outputs, schakelbaar aan/uit |
| **DIGITAL_INPUT** | `binary_sensor` | 24V digitale inputs, on/off status |
| **PROGRAMMABLE_DIO (Output)** | `switch` | Programmeerbare outputs, schakelbaar |
| **PROGRAMMABLE_DIO (Input)** | `binary_sensor` | Programmeerbare inputs, status |
| **ANALOG_INPUT_VOLTAGE** | `sensor` | 0-10V analoge input, eenheid: V |
| **ANALOG_INPUT_CURRENT** | `sensor` | 4-20mA analoge input, eenheid: mA |
| **ANALOG_OUTPUT_VOLTAGE** | `number` | 0-10V analoge output, instelbaar |
| **ANALOG_OUTPUT_CURRENT** | `number` | 4-20mA analoge output, instelbaar |
| **RTD_INPUT** | `sensor` | RTD temperatuur sensor, eenheid: °C |
| **THERMOCOUPLE_INPUT** | `sensor` | Thermokoppel sensor, eenheid: °C |
| **ENCODER_INPUT** | `sensor` | Encoder pulsen teller |

## MQTT Topics

### Discovery Topics

Discovery configuraties worden gepubliceerd naar:
```
homeassistant/<component>/<device_id>/<object_id>/config
```

**Voorbeelden:**
```
homeassistant/switch/portenta_h7/relay1/config
homeassistant/binary_sensor/portenta_h7/prog_in1/config
homeassistant/sensor/portenta_h7/analog_in1/config
homeassistant/number/portenta_h7/analog_out1/config
```

### State Topics

Pin states worden gepubliceerd naar:
```
portenta/<pin_topic>/state
```

**Voorbeelden:**
```
portenta/relay1/state          → "1.000" or "0.000"
portenta/prog_in1/state        → "1.000" or "0.000"
portenta/analog_in1/state      → "5.234" (voltage)
```

### Command Topics

Voor outputs (switches en numbers) kan Home Assistant commands sturen naar:
```
portenta/<pin_topic>/set
```

**Voorbeelden:**
```
portenta/relay1/set            → "1" (aan) of "0" (uit)
portenta/analog_out1/set       → "7.5" (voltage waarde)
```

### Availability Topic

De availability status wordt gepubliceerd naar:
```
portenta/portenta_h7/availability
```

Waarden:
- `online` - Portenta is verbonden
- `offline` - Portenta is offline (via Last Will Testament)

## Device Info

Alle entities zijn gegroepeerd onder één device met de volgende informatie:

```json
{
  "identifiers": ["portenta_h7"],
  "name": "Portenta Machine Control",
  "manufacturer": "Arduino",
  "model": "Portenta H7 + Machine Control",
  "sw_version": "1.0.0"
}
```

## Gebruik in Home Assistant

### Entities Bekijken

1. Ga naar **Settings** → **Devices & Services** → **MQTT**
2. Zoek naar "Portenta Machine Control"
3. Klik op het device om alle entities te zien

### Voorbeeld Dashboard

Voeg entities toe aan je dashboard:

```yaml
type: entities
title: Portenta I/O
entities:
  - entity: switch.portenta_relay_1
    name: Relay 1
  - entity: binary_sensor.portenta_prog_in_1
    name: Programmable Input 1
  - entity: sensor.portenta_analog_in_1
    name: Voltage Input 1
  - entity: number.portenta_analog_out_1
    name: Voltage Output 1
```

### Automation Voorbeeld

Schakel een relay aan wanneer een input actief wordt:

```yaml
automation:
  - alias: "Turn on Relay when Input Active"
    trigger:
      - platform: state
        entity_id: binary_sensor.portenta_prog_in_1
        to: "on"
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.portenta_relay_1
```

### Script Voorbeeld

Stel een analoge output waarde in:

```yaml
script:
  set_analog_output:
    alias: "Set Analog Output to 5V"
    sequence:
      - service: number.set_value
        target:
          entity_id: number.portenta_analog_out_1
        data:
          value: 5.0
```

## Troubleshooting

### Entities Verschijnen Niet

1. **Check MQTT Discovery status:**
   ```bash
   # Luister naar discovery messages
   mosquitto_sub -h <broker_ip> -t "homeassistant/#" -v
   ```

2. **Check Portenta logs:**
   - Open Serial Monitor (115200 baud)
   - Kijk naar "=== Publishing Home Assistant Discovery ==="
   - Check voor errors

3. **Herstart Home Assistant:**
   - Settings → System → Restart

### Entities Tonen "Unavailable"

1. **Check availability topic:**
   ```bash
   mosquitto_sub -h <broker_ip> -t "portenta/portenta_h7/availability" -v
   ```
   Moet "online" tonen

2. **Check state topics:**
   ```bash
   mosquitto_sub -h <broker_ip> -t "portenta/#" -v
   ```
   Moet elke 5 seconden updates tonen

### Discovery Opnieuw Publiceren

Als je de pin configuratie hebt gewijzigd:

**Optie 1: Hot-Reload via MQTT (Aanbevolen - Geen Herstart!)**
```bash
# Stuur nieuwe configuratie via MQTT
mosquitto_pub -h <broker_ip> -t "portenta/config" -f config.json

# Of met één regel JSON:
mosquitto_pub -h <broker_ip> -t "portenta/config" -m '{"pins":[...]}'
```

De Portenta zal:
1. Configuratie valideren
2. Alle bestaande pins stoppen
3. Nieuwe configuratie laden
4. Pins opnieuw initialiseren
5. Home Assistant Discovery updaten
6. Alle states publiceren

**Totale tijd: ~2-3 seconden, geen herstart nodig!**

**Optie 2: Via Herstart**
1. Update config.json bestand
2. Hercompileer en upload firmware
3. Discovery messages worden automatisch gepubliceerd bij boot

### Oude Entities Verwijderen

Als je pins hebt verwijderd maar entities blijven bestaan:

1. Ga naar **Settings** → **Devices & Services** → **MQTT**
2. Vind het device "Portenta Machine Control"
3. Klik op de entity
4. Klik **Delete**

Of gebruik MQTT om discovery te verwijderen:
```bash
mosquitto_pub -h <broker_ip> -t "homeassistant/switch/portenta_h7/relay1/config" -m "" -r
```

## Geavanceerde Configuratie

### Custom Device ID

Wijzig in `main.cpp`:
```cpp
const char* HA_DEVICE_ID = "portenta_h7";  // Pas aan voor unieke identificatie
const char* HA_DEVICE_NAME = "My Portenta";
```

### Custom Discovery Prefix

Wijzig in `HADiscovery.cpp`:
```cpp
static constexpr const char* HA_DISCOVERY_PREFIX = "homeassistant";  // Pas aan indien nodig
```

### Device Info Aanpassen

Wijzig in `HADiscovery.cpp` de `buildDeviceInfo()` functie:
```cpp
deviceObj["name"] = deviceName;
deviceObj["manufacturer"] = "Arduino";
deviceObj["model"] = "Portenta H7 + Machine Control";
deviceObj["sw_version"] = "1.0.0";  // Update versie
```

## Voordelen van MQTT Discovery

✅ **Zero configuratie** - Geen handmatige MQTT sensor configuraties in YAML
✅ **Automatische updates** - Pin wijzigingen worden automatisch gesynchroniseerd
✅ **Device grouping** - Alle I/O's netjes gegroepeerd onder één device
✅ **Availability tracking** - Zie direct of de Portenta online is
✅ **Metadata** - Units, device classes, min/max waarden automatisch ingesteld
✅ **User-friendly** - Mooie namen en iconen in de UI

## Voorbeeld Output

Bij succesvolle discovery zie je in de Serial Monitor:

```
=== Publishing Home Assistant Discovery ===
Device ID: portenta_h7
Device Name: Portenta Machine Control
Total pins to publish: 14
Publishing switch: relay_1 (relay1)... OK
Publishing switch: relay_2 (relay2)... OK
Publishing binary_sensor: prog_in_1 (prog_in1)... OK
Publishing sensor: analog_in_1 (analog_in1)... OK
Publishing number: analog_out_1 (analog_out1)... OK
...

=== Discovery Summary ===
Success: 14
Failed: 0
```

## Zie Ook

- [Home Assistant MQTT Discovery Documentation](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery)
- [MQTT Integration Guide](https://www.home-assistant.io/integrations/mqtt/)
- [Portenta Configuration Guide](TECHNICAL_SPECIFICATION.md)
