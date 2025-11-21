# Runtime Configuration - Hot-Reload

De Portenta MQTT Controller ondersteunt **hot-reload** - je kunt de configuratie live aanpassen zonder het apparaat te herstarten!

## Quick Start

**Wijzig configuratie in 3 stappen:**

```bash
# 1. (Optioneel) Monitor feedback
mosquitto_sub -h <broker> -t "portenta/config/status" -v &

# 2. Stuur nieuwe configuratie
mosquitto_pub -h <broker> -t "portenta/config" -f data/config.json

# 3. Zie directe feedback
# Output: validating
#         success: 34 pins active
```

**Klaar!** Alle pins zijn live bijgewerkt, Home Assistant entities zijn gesynchroniseerd.

## Overzicht

**Hot-reload features:**
- ✅ Wijzig pin modes (INPUT ↔ OUTPUT)
- ✅ Voeg nieuwe pins toe
- ✅ Verwijder bestaande pins
- ✅ Pas sampling rates aan
- ✅ Update MQTT topics
- ✅ Wijzig calibratie (scale, offset)
- ✅ **Geen herstart nodig** - duurt ~2-3 seconden
- ✅ Home Assistant entities worden automatisch bijgewerkt

## Hoe Werkt Het?

### Via MQTT

**Stap 1: Maak een nieuwe configuratie**

Edit je `config.json` met de gewenste wijzigingen. Bijvoorbeeld, verander een programmable DIO van INPUT naar OUTPUT:

```json
{
  "pinNumber": 0,
  "type": "PROGRAMMABLE_DIO",
  "name": "prog_dio_0",
  "mqttTopic": "prog_dio_0",
  "mode": "OUTPUT"  ← Was "INPUT"
}
```

**Stap 2: Monitor status feedback (optioneel, in tweede terminal)**

```bash
# Start MQTT monitor voor directe feedback
mosquitto_sub -h <broker_ip> -t "portenta/config/status" -v
```

**Stap 3: Stuur de configuratie via MQTT**

```bash
# Vanaf bestand
mosquitto_pub -h <broker_ip> -t "portenta/config" -f data/config.json

# Of direct inline (korte versie)
mosquitto_pub -h <broker_ip> -t "portenta/config" -m '{"pins":[{"pinNumber":0,"type":"PROGRAMMABLE_DIO","name":"prog_dio_0","mqttTopic":"prog_dio_0","mode":"OUTPUT"}]}'
```

Je zult direct feedback zien op de status topic:
```
portenta/config/status validating
portenta/config/status success: 34 pins active
```

Bij foutieve JSON:
```
portenta/config/status error: invalid JSON
```

**Stap 4: Volg de details in Serial Monitor (optioneel)**

```
=== Updating configuration from MQTT ===
Configuration JSON validated successfully!
Applying new configuration with hot-reload...

=== Hot-Reloading Configuration ===
Step 1: Shutting down existing pin handlers...
  All pins stopped
Step 2: Re-initializing I/O controller...
  I/O controller ready
Step 3: Restoring MQTT callback...
  Callback restored
Step 4: Loading new pin configuration...
  Loaded 34 pins (0 failed)
Step 5: Updating Home Assistant Discovery...
  Publishing switch: prog_dio_0 (prog_dio_0)... OK
  Discovery updated
Step 6: Publishing initial pin states...
  Published 18 inputs, 16 outputs
  Initial states published

=== Configuration Reload Complete ===
Total pins active: 34
Configuration applied! No restart required.
```

**Stap 5: Check Home Assistant**

- Ga naar **Settings** → **Devices & Services** → **MQTT**
- Open "Portenta Machine Control"
- Entities zijn automatisch bijgewerkt!
- `prog_dio_0` is nu een **switch** in plaats van een **binary_sensor**

## Use Cases

### 1. Wijzig Pin Mode (INPUT → OUTPUT)

**Scenario:** Je hebt een sensor aangesloten op prog_dio_0, maar wil nu een actuator aansturen.

```json
// Voor: INPUT
{"pinNumber":0,"type":"PROGRAMMABLE_DIO","mode":"INPUT"}

// Na: OUTPUT
{"pinNumber":0,"type":"PROGRAMMABLE_DIO","mode":"OUTPUT"}
```

**Effect in Home Assistant:**
- `binary_sensor.prog_dio_0` → wordt `switch.prog_dio_0`
- Topic `portenta/prog_dio_0/state` blijft hetzelfde
- Topic `portenta/prog_dio_0/set` wordt actief

### 2. Voeg Extra Pins Toe

**Scenario:** Je hebt extra I/O nodig voor een nieuwe sensor.

Voeg gewoon een nieuwe pin entry toe aan de JSON:

```json
{
  "pinNumber": 3,
  "type": "ANALOG_INPUT_VOLTAGE",
  "name": "pressure_sensor",
  "mqttTopic": "pressure",
  "mode": "INPUT",
  "sampleRateMs": 50,
  "minValue": 0.0,
  "maxValue": 10.0
}
```

### 3. Pas Sampling Rate Aan

**Scenario:** Je input ruis vertoont, verhoog de sample time.

```json
// Voor: 100ms sample rate
{"sampleRateMs": 100}

// Na: 500ms sample rate (trager, maar stabieler)
{"sampleRateMs": 500}
```

### 4. Verander MQTT Topic

**Scenario:** Je wilt betere topic namen.

```json
// Voor
{"mqttTopic": "relay0"}

// Na
{"mqttTopic": "heater_pump"}
```

Home Assistant zal automatisch:
- Oude entity verwijderen (`switch.relay0`)
- Nieuwe entity maken (`switch.heater_pump`)

### 5. Calibreer Analoge Inputs

**Scenario:** Je voltage sensor meet 5.2V maar moet 5.0V zijn.

```json
{
  "type": "ANALOG_INPUT_VOLTAGE",
  "scaleFactor": 0.96,  // 5.0 / 5.2 = ~0.96
  "offset": 0.0
}
```

## Workflow voor Productie

### Ontwikkeling
1. Test configuratie lokaal
2. Valideer met Serial Monitor
3. Check dat alle pins correct werken

### Deployment
```bash
# Stuur naar productie device via MQTT
mosquitto_pub -h production.broker.com -t "portenta/config" -f config_v2.json
```

### Rollback Bij Problemen
```bash
# Stuur oude configuratie terug
mosquitto_pub -h production.broker.com -t "portenta/config" -f config_v1_backup.json
```

## Limitaties

### Wat WEL kan tijdens hot-reload:
- ✅ Mode wijzigen (INPUT/OUTPUT)
- ✅ Pins toevoegen/verwijderen
- ✅ Topics hernoemen
- ✅ Sampling rates aanpassen
- ✅ Calibratie wijzigen

### Wat NIET kan zonder herstart:
- ❌ MQTT broker adres wijzigen (in credentials.h)
- ❌ WiFi/Ethernet credentials wijzigen
- ❌ Device ID wijzigen
- ❌ MQTT buffer size wijzigen

Voor deze wijzigingen moet je de firmware hercompileren en uploaden.

## Foutafhandeling

### Ongeldige JSON

```
=== Updating configuration from MQTT ===
Configuration update failed - invalid JSON or validation error
```

**Oplossing:**
1. Valideer je JSON met een online tool (jsonlint.com)
2. Check de Serial Monitor voor details
3. Stuur correcte configuratie opnieuw

### Pin Configuratie Fout

```
Step 4: Loading new pin configuration...
  FAIL: invalid_pin_99
  Loaded 33 pins (1 failed)
```

**Oplossing:**
1. Check pin nummers (max waarden in docs)
2. Check dat pin types kloppen
3. Verwijder problematische pin uit JSON

### MQTT Timeout

Als hot-reload langer dan 5 seconden duurt:

**Mogelijke oorzaken:**
- Zeer grote configuratie (>50 pins)
- Home Assistant discovery traag
- MQTT broker overbelast

**Normaal:** Discovery van 34 pins duurt ~2 seconden

## Best Practices

### 1. Bewaar Config Versies
```bash
config_v1.0_baseline.json
config_v1.1_added_temp_sensors.json
config_v1.2_production.json
```

### 2. Test Eerst Op Ontwikkel Device

Stuur nooit direct naar productie!

```bash
# Test omgeving
mosquitto_pub -h 192.168.1.100 -t "portenta/config" -f config_test.json

# Na validatie -> productie
mosquitto_pub -h production.local -t "portenta/config" -f config_test.json
```

### 3. Monitor Serial Output

Gebruik `pio device monitor` tijdens hot-reload om problemen direct te zien.

### 4. Minimale Wijzigingen Per Keer

Wijzig niet alles tegelijk! Maak kleine, incrementele changes:
- Eerst 1 pin testen
- Dan meerdere pins
- Dan volledige configuratie

## Voorbeeld: Complete Workflow

**Doel:** Verander prog_dio_0 van sensor input naar relay output

**Stap 1: Backup huidige config**
```bash
mosquitto_sub -h broker -t "portenta/status/detail" -C 1 | jq .pins > backup.json
```

**Stap 2: Edit config.json**
```json
{
  "pinNumber": 0,
  "type": "PROGRAMMABLE_DIO",
  "name": "prog_dio_0",
  "mqttTopic": "prog_dio_0",
  "mode": "OUTPUT"  // Was INPUT
}
```

**Stap 3: Apply via MQTT**
```bash
mosquitto_pub -h broker -t "portenta/config" -f data/config.json
```

**Stap 4: Verify in Home Assistant**
- Open MQTT integration
- Check dat `prog_dio_0` nu een **switch** is
- Test schakelaar (moet relay aansturen)

**Stap 5: Test functionaliteit**
```bash
# Schakel aan
mosquitto_pub -h broker -t "portenta/prog_dio_0/set" -m "1"

# Schakel uit
mosquitto_pub -h broker -t "portenta/prog_dio_0/set" -m "0"
```

**Succes!** 🎉 - Geen herstart nodig, alles werkt direct!

## Troubleshooting

### Hot-reload werkt niet

**Check 1: MQTT verbinding**
```bash
mosquitto_sub -h broker -t "portenta/status" -v
```
Moet "online" tonen.

**Check 2: Topic correct**
Moet exact zijn: `portenta/config` (niet `portenta/config/update` of iets anders)

**Check 3: JSON payload**
```bash
# Test met simpele config
mosquitto_pub -h broker -t "portenta/config" -m '{"pins":[]}'
```

Moet in Serial Monitor tonen:
```
=== Updating configuration from MQTT ===
Configuration JSON validated successfully!
```

### Entities niet bijgewerkt in HA

**Oplossing 1: Force refresh**
```bash
# Herstart Home Assistant
Developer Tools → YAML → Restart
```

**Oplossing 2: Herstart MQTT integration**
1. Settings → Devices & Services → MQTT
2. **⋮** (menu) → Reload

**Oplossing 3: Verwijder oude entities**
1. Settings → Devices & Services → MQTT → "Portenta Machine Control"
2. Verwijder conflicterende entities
3. Wacht 30 seconden
4. Nieuwe entities verschijnen automatisch

## Persistente Configuratie (QSPI Flash)

### Overzicht

Sinds versie 1.0 wordt de configuratie automatisch opgeslagen in het **QSPI flash geheugen**. Dit betekent:

- ✅ **Configuratie blijft behouden** na reboot/power cycle
- ✅ **Configuratie blijft behouden** na firmware updates (tenzij versie-upgrade)
- ✅ **Automatische migratie** bij nieuwere embedded configuratie
- ✅ **Factory reset** optie om terug te keren naar defaults

### Hoe Werkt Persistente Opslag?

#### Bij Eerste Keer Opstarten
```
1. ConfigManager probeert config.json te laden van QSPI flash
2. File niet gevonden → laadt embedded defaults (data/config.json)
3. Configuratie staat nu in RAM (niet automatisch opgeslagen naar QSPI)
```

#### Bij Volgende Reboots
```
1. ConfigManager laadt config.json van QSPI flash
2. Versie check: embedded vs QSPI
3. Als embedded nieuwer → auto-migratie
4. Anders → gebruiker configuratie behouden
```

#### Bij MQTT Config Update
```
1. Nieuwe configuratie ontvangen via portenta/config
2. JSON gevalideerd en toegepast (hot-reload)
3. Automatisch opgeslagen naar QSPI flash
4. Blijft behouden na reboot!
```

### Config Versioning

Elke configuratie heeft een **version** nummer:

```json
{
  "version": 1,
  "pins": [...]
}
```

**Wanneer verhoog je de versie?**
- Nieuwe pin types toegevoegd
- Nieuwe configuratie velden toegevoegd
- Breaking changes in configuratie structuur

**Voorbeeld upgrade scenario:**

```
Firmware v1.0 met config version 1 → installed op device
User wijzigt pins via MQTT → opgeslagen in QSPI als version 1

Firmware update naar v2.0 met config version 2:
1. Device boot → laadt QSPI config (version 1)
2. Check: embedded version (2) > QSPI version (1)
3. Auto-migratie! Nieuwe defaults geladen en opgeslagen
4. User krijgt nieuwe features automatisch
```

**Serial output bij auto-migratie:**
```
Config loaded from QSPI flash (version 1)
⚠ Embedded config version 2 is newer than QSPI version 1
Auto-migrating to newer configuration...
✓ Configuration upgraded successfully
```

### Factory Reset

**Via API (toekomstig):**
```bash
curl -X POST http://<device_ip>/api/factoryReset
```

**Via MQTT (toekomstig):**
```bash
mosquitto_pub -h broker -t "portenta/system/factoryReset" -m "1"
```

**Wat gebeurt er:**
1. Embedded defaults geladen (data/config.json)
2. Opgeslagen naar QSPI flash
3. Hot-reload toegepast
4. User configuratie volledig vervangen door factory defaults

**Serial output:**
```
=== Factory Reset ===
Resetting to embedded default configuration...
Config saved to QSPI flash (4363 bytes)
Factory reset complete!
Configuration restored to version 1
```

### Storage Locatie

```
QSPI Flash Partitie 4 (User Data)
├── config.json       ← Runtime pin configuratie (dit document)
├── settings.json     ← Network settings (SSID, MQTT broker)
└── logs/             ← System logs
```

### Best Practices voor Persistente Config

#### 1. Backup Maken Van Configuratie
```bash
# Lees huidige configuratie van device
mosquitto_sub -h broker -t "portenta/config" -C 1 > backup_config.json
```

#### 2. Test Voor Je Update
```bash
# Test nieuwe config
mosquitto_pub -h broker -t "portenta/config" -f new_config.json

# Als het werkt: opgeslagen in QSPI, blijft behouden
# Als het NIET werkt: stuur backup terug
mosquitto_pub -h broker -t "portenta/config" -f backup_config.json
```

#### 3. Documenteer Je Wijzigingen
```bash
# Versie nummering in filename
my_configs/
├── config_v1.0_baseline.json
├── config_v1.1_added_temp_sensor.json
├── config_v1.2_production.json
```

### Troubleshooting Persistente Opslag

#### Config wordt niet opgeslagen
**Symptoom:** Na reboot zijn wijzigingen verdwenen

**Check Serial Monitor:**
```
Failed to save config to storage
Cannot access user partition for config
```

**Oplossing:**
1. QSPI partitie formateren (automatisch bij volgende config update)
2. Of: factory reset triggeren

#### Config blijft oude waarde houden na firmware update
**Symptoom:** Nieuwe firmware features werken niet

**Oorzaak:** User config in QSPI heeft voorrang

**Oplossing 1:** Increment version in data/config.json (aanbevolen)
```json
{
  "version": 2,  // Was 1
  "pins": [...]
}
```

**Oplossing 2:** Factory reset na firmware update

#### Hoe weet ik welke config actief is?
**Check Serial Monitor bij boot:**
```
Config loaded from QSPI flash (version 1)
```

Of:
```
No config.json found in flash storage
Loading default config instead
Loading default embedded config...
```

### Voordelen van Persistente Opslag

**Voor Development:**
- Test configuraties blijven behouden tijdens development
- Geen handmatige configuratie na elke reboot
- Snellere iteratie tijdens testing

**Voor Production:**
- Site-specifieke configuraties blijven behouden
- Firmware updates zonder configuratie verlies
- Calibratie waarden blijven behouden

**Voor Maintenance:**
- Remote configuratie updates via MQTT
- Factory reset optie bij problemen
- Configuratie backup en restore mogelijk

## Zie Ook

- [Home Assistant Integration](HOME_ASSISTANT_INTEGRATION.md) - Discovery setup
- [QSPI Flash Setup](QSPI_FLASH_SETUP.md) - Low-level QSPI configuratie
- [QSPI Flash Explained](QSPI_FLASH_EXPLAINED.md) - Technische details
- [Configuration Guide](../data/config.json) - Voorbeeld configuratie
