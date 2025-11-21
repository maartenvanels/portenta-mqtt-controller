# MQTT Topics Configuration

De MQTT client laadt topics nu uit `appsettings.json` in plaats van hardcoded in de code.

## Configuratie

### appsettings.json

Voeg de topics toe aan de `MqttSettings:Topics` array:

```json
{
  "MqttSettings": {
    "BrokerAddress": "192.168.1.100",
    "BrokerPort": 1883,
    "Username": "your-username",
    "Password": "your-password",
    "Topics": [
      "portenta/digital_in1/state"
    ]
  }
}
```

### Meerdere Topics

Je kunt makkelijk meerdere topics toevoegen:

```json
{
  "MqttSettings": {
    "Topics": [
      "portenta/digital_in1/state",
      "portenta/digital_in2/state",
      "portenta/analog_in0/state",
      "portenta/status",
      "portenta/status/detail"
    ]
  }
}
```

### Wildcards

MQTT wildcards worden ondersteund:

```json
{
  "MqttSettings": {
    "Topics": [
      "portenta/+/state",              // Alle single-level state topics
      "portenta/digital_in+/state",    // Alle digital inputs
      "portenta/#"                     // Alle Portenta topics
    ]
  }
}
```

**MQTT Wildcard Syntax:**
- `+` matched exact één level: `portenta/+/state` → `portenta/digital_in1/state` ✅
- `#` matched alle remaining levels (alleen aan eind!): `portenta/#` → `portenta/digital_in1/state` ✅

## Voorbeelden

### Voorbeeld 1: Enkele Digital Input

```json
{
  "MqttSettings": {
    "Topics": [
      "portenta/digital_in1/state"
    ]
  }
}
```

**Output:**
```
Subscribed to 1 topic(s):
  - portenta/digital_in1/state

[2025-10-29 15:30:12.345] digital_in1: HIGH (1) [1.000]
```

### Voorbeeld 2: Alle Digital Inputs

```json
{
  "MqttSettings": {
    "Topics": [
      "portenta/digital_in1/state",
      "portenta/digital_in2/state",
      "portenta/digital_in3/state",
      "portenta/digital_in4/state",
      "portenta/digital_in5/state",
      "portenta/digital_in6/state",
      "portenta/digital_in7/state",
      "portenta/digital_in8/state"
    ]
  }
}
```

### Voorbeeld 3: Digital + Analog Inputs

```json
{
  "MqttSettings": {
    "Topics": [
      "portenta/digital_in1/state",
      "portenta/digital_in2/state",
      "portenta/analog_in0/state",
      "portenta/analog_in1/state",
      "portenta/analog_in2/state"
    ]
  }
}
```

**Output:**
```
Subscribed to 5 topic(s):
  - portenta/digital_in1/state
  - portenta/digital_in2/state
  - portenta/analog_in0/state
  - portenta/analog_in1/state
  - portenta/analog_in2/state

[2025-10-29 15:30:12.345] digital_in1: HIGH (1) [1.000]
[2025-10-29 15:30:13.456] analog_in0: 5.234V
[2025-10-29 15:30:14.567] digital_in2: LOW (0) [0.000]
```

### Voorbeeld 4: Met Wildcards

```json
{
  "MqttSettings": {
    "Topics": [
      "portenta/+/state",      // Alle single-level states
      "portenta/status"        // System status
    ]
  }
}
```

**Matched topics:**
- `portenta/digital_in1/state` ✅
- `portenta/analog_in0/state` ✅
- `portenta/relay0/state` ✅
- `portenta/status` ✅
- `portenta/prog_dio/0/state` ❌ (heeft 2 levels na portenta/)

### Voorbeeld 5: Alle Portenta Topics

```json
{
  "MqttSettings": {
    "Topics": [
      "portenta/#"
    ]
  }
}
```

**Dit matched ALLES:**
- Alle inputs
- Alle outputs
- Status topics
- Control topics

## Topic Structuur

De Portenta controller gebruikt deze topic structuur:

### Input Topics (State)
| Topic | Beschrijving |
|-------|--------------|
| `portenta/digital_in1/state` | Digital input 1 (8 totaal: 1-8) |
| `portenta/analog_in0/state` | Analog voltage input 0 (3 totaal: 0-2, 0-10V) |
| `portenta/prog_dio/0/state` | Programmable DIO 0 (12 totaal: 0-11) |

### Output Topics (Set)
| Topic | Beschrijving |
|-------|--------------|
| `portenta/relay0/set` | Relay output 0 (8 totaal: 0-7) |
| `portenta/analog_out0/set` | Analog voltage output 0 (4 totaal: 0-3, 0-10V) |
| `portenta/prog_dio/6/set` | Programmable DIO 6 (6-11 zijn outputs) |

### Status Topics
| Topic | Beschrijving |
|-------|--------------|
| `portenta/status` | Online/offline status (LWT) |
| `portenta/status/detail` | Gedetailleerde JSON status |

## Environment Variables

Je kunt topics ook via environment variables instellen:

```bash
# PowerShell
$env:MqttSettings__Topics__0 = "portenta/digital_in1/state"
$env:MqttSettings__Topics__1 = "portenta/digital_in2/state"
dotnet run

# Linux/macOS
export MqttSettings__Topics__0="portenta/digital_in1/state"
export MqttSettings__Topics__1="portenta/digital_in2/state"
dotnet run
```

## appsettings.Local.json

Voor persoonlijke configuratie (niet in git):

```json
{
  "MqttSettings": {
    "Topics": [
      "portenta/digital_in1/state",
      "portenta/analog_in0/state"
    ]
  }
}
```

Dit overschrijft de topics uit `appsettings.json`.

## Geen Topics Geconfigureerd

Als er geen topics zijn geconfigureerd, zie je:

```
Warning: No topics configured in appsettings.json
Add topics to MqttSettings:Topics array
```

## Tips

1. **Start klein:** Begin met 1-2 topics en breid uit als je meer wilt monitoren
2. **Gebruik wildcards:** Voor veel topics, gebruik `+` of `#` wildcards
3. **Filter in code:** Subscribe op alles met `#`, filter in de `OnMessageReceived` handler
4. **Test eerst:** Gebruik `mosquitto_sub` om te zien welke topics er gepubliceerd worden:
   ```bash
   mosquitto_sub -h 192.168.1.100 -t "portenta/#" -v
   ```

## Troubleshooting

### "Subscribe error: The character '#' is only allowed as last character"
❌ Fout: `portenta/digital_in#/state`
✅ Correct: `portenta/+/state` of `portenta/#`

### Geen messages ontvangen
1. Check of de Portenta controller online is
2. Test met mosquitto_sub: `mosquitto_sub -h <broker> -t "portenta/#" -v`
3. Controleer topic spelling in config (case-sensitive!)
4. Check MQTT broker logs

### Topics uit config worden genegeerd
- Check JSON syntax (comma's, quotes)
- Restart de client na config changes
- Check console output voor "Subscribed to X topic(s)"
