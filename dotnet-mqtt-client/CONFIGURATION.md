# Configuration Guide - Portenta MQTT Client

## Overzicht

De .NET MQTT client ondersteunt flexibele configuratie via meerdere bronnen. Dit document beschrijft alle configuratiemogelijkheden en wanneer je welke moet gebruiken.

## Configuratie Bronnen (Prioriteit)

De client laadt configuratie in deze volgorde (laatste wint):

1. **appsettings.json** - Basis configuratie (gecommit in git)
2. **appsettings.Local.json** - Lokale overrides (NIET in git)
3. **Environment Variables** - Runtime overrides
4. **Command-line Arguments** - Hoogste prioriteit

## 1. appsettings.json

**Locatie:** `PortentaMqttClient/appsettings.json`
**Gebruik:** Basis configuratie die wordt gedeeld in version control
**Commit in Git:** ✅ Ja

```json
{
  "MqttSettings": {
    "BrokerAddress": "192.168.1.100",
    "BrokerPort": 1883,
    "Username": "your-username",
    "Password": "your-password",
    "ClientIdPrefix": "PortentaMqttClient",
    "ReconnectDelaySeconds": 5
  },
  "Subscriptions": {
    "DigitalInputs": true,
    "AnalogInputs": true,
    "ProgrammableDIO": true,
    "SystemStatus": true,
    "DetailedStatus": true
  },
  "Display": {
    "ShowTimestamps": true,
    "ColorCodedOutput": true,
    "VerboseLogging": false
  }
}
```

**Gebruik dit voor:**
- Default waarden
- Gedeelde team configuratie
- Development environment defaults

**NIET gebruiken voor:**
- Production credentials (zie Security.md)
- Persoonlijke/gevoelige informatie

## 2. appsettings.Local.json

**Locatie:** `PortentaMqttClient/appsettings.Local.json`
**Gebruik:** Persoonlijke lokale configuratie
**Commit in Git:** ❌ NEE (staat in .gitignore)

### Setup

1. Kopieer de template:
```bash
cp appsettings.Local.json.template appsettings.Local.json
```

2. Edit met je persoonlijke instellingen:
```json
{
  "MqttSettings": {
    "BrokerAddress": "192.168.1.100",
    "Username": "mijn-username",
    "Password": "mijn-secret-password"
  }
}
```

3. Dit bestand overschrijft waarden uit `appsettings.json`

**Gebruik dit voor:**
- Persoonlijke MQTT credentials
- Lokale broker IP adressen
- Development testing configuraties
- Per-developer settings

**Voordelen:**
- ✅ Wordt NIET gecommit (veilig voor credentials)
- ✅ Overschrijft alleen specifieke waarden
- ✅ Andere settings blijven uit appsettings.json
- ✅ Elke developer kan eigen configuratie hebben

## 3. Environment Variables

**Gebruik:** Production deployments, Docker, CI/CD
**Security:** 🟢 Aanbevolen voor production

### Windows PowerShell
```powershell
$env:MqttSettings__BrokerAddress = "192.168.1.100"
$env:MqttSettings__BrokerPort = "1883"
$env:MqttSettings__Username = "your-username"
$env:MqttSettings__Password = "your-password"

dotnet run
```

### Windows CMD
```cmd
set MqttSettings__BrokerAddress=192.168.1.100
set MqttSettings__BrokerPort=1883
set MqttSettings__Username=your-username
set MqttSettings__Password=your-password

dotnet run
```

### Linux/macOS Bash
```bash
export MqttSettings__BrokerAddress="192.168.1.100"
export MqttSettings__BrokerPort="1883"
export MqttSettings__Username="your-username"
export MqttSettings__Password="your-password"

dotnet run
```

### Docker / Docker Compose
```yaml
services:
  mqtt-client:
    image: portenta-mqtt-client:latest
    environment:
      - MqttSettings__BrokerAddress=mqtt-broker
      - MqttSettings__BrokerPort=1883
      - MqttSettings__Username=${MQTT_USER}
      - MqttSettings__Password=${MQTT_PASSWORD}  # Van .env file
```

### Systemd Service (Linux)
```ini
[Service]
Environment="MqttSettings__BrokerAddress=192.168.1.100"
Environment="MqttSettings__Username=your-username"
Environment="MqttSettings__Password=your-password"
ExecStart=/usr/bin/dotnet /opt/mqtt-client/PortentaMqttClient.dll
```

**Gebruik dit voor:**
- ✅ Production deployments
- ✅ Docker containers
- ✅ CI/CD pipelines
- ✅ Cloud deployments (Azure, AWS, etc.)

## 4. Command-line Arguments

**Gebruik:** Quick testing, scripts
**Security:** ⚠️ Vermijd voor credentials (zichtbaar in process list)

```bash
# Syntax: dotnet run [broker-address] [port]
dotnet run 192.168.1.100 1883
```

**Gebruik dit voor:**
- ✅ Quick testing met verschillende brokers
- ✅ Scripts/automation (alleen broker address)
- ❌ NIET voor credentials (blijven in shell history)

## Configuratie Voorbeelden

### Scenario 1: Lokale Development
```bash
# 1. Edit appsettings.Local.json met je credentials
{
  "MqttSettings": {
    "Username": "dev-user",
    "Password": "dev-password"
  }
}

# 2. Run
cd PortentaMqttClient
dotnet run
```

### Scenario 2: Test met verschillende broker
```bash
# Gebruik command-line argument
dotnet run 192.168.1.50 1883
```

### Scenario 3: Production Docker
```yaml
# docker-compose.yml
version: '3.8'
services:
  mqtt-client:
    build: .
    environment:
      - MqttSettings__BrokerAddress=mqtt-broker
      - MqttSettings__Username=${MQTT_USER}
      - MqttSettings__Password=${MQTT_PASS}
    restart: unless-stopped

# .env file (NIET in git)
MQTT_USER=production-user
MQTT_PASS=super-secure-password
```

### Scenario 4: CI/CD Pipeline
```yaml
# GitHub Actions / Azure DevOps
- name: Run MQTT Client
  env:
    MqttSettings__BrokerAddress: ${{ secrets.MQTT_BROKER }}
    MqttSettings__Username: ${{ secrets.MQTT_USER }}
    MqttSettings__Password: ${{ secrets.MQTT_PASSWORD }}
  run: dotnet run
```

## Configuratie Keys

### MQTT Settings

| Key | Type | Default | Beschrijving |
|-----|------|---------|--------------|
| `BrokerAddress` | string | "192.168.18.74" | MQTT broker IP of hostname |
| `BrokerPort` | int | 1883 | MQTT broker poort |
| `Username` | string | null | MQTT authenticatie username |
| `Password` | string | null | MQTT authenticatie password |
| `ClientIdPrefix` | string | "PortentaMqttClient" | Prefix voor MQTT client ID |
| `ReconnectDelaySeconds` | int | 5 | Seconden tussen reconnect pogingen |

### Subscription Settings

| Key | Type | Default | Beschrijving |
|-----|------|---------|--------------|
| `DigitalInputs` | bool | true | Subscribe to digital input topics |
| `AnalogInputs` | bool | true | Subscribe to analog input topics |
| `ProgrammableDIO` | bool | true | Subscribe to programmable DIO topics |
| `SystemStatus` | bool | true | Subscribe to system status |
| `DetailedStatus` | bool | true | Subscribe to detailed status |

### Display Settings

| Key | Type | Default | Beschrijving |
|-----|------|---------|--------------|
| `ShowTimestamps` | bool | true | Toon timestamps bij messages |
| `ColorCodedOutput` | bool | true | Gebruik kleuren in console output |
| `VerboseLogging` | bool | false | Extra debug logging |

## Environment Variable Format

Environment variables gebruiken `__` (dubbele underscore) als separator:

```
MqttSettings__BrokerAddress     → MqttSettings:BrokerAddress
MqttSettings__Username          → MqttSettings:Username
Subscriptions__DigitalInputs    → Subscriptions:DigitalInputs
Display__ShowTimestamps         → Display:ShowTimestamps
```

## Debugging Configuratie

Om te zien welke configuratie wordt geladen, check de console output:

```
=== Portenta MQTT Client ===
Subscribing to digital inputs and state changes

Configuration loaded: 192.168.1.100:1883 (user: mqtt-user)
Connecting to MQTT Broker: 192.168.1.100:1883
```

## Best Practices

1. ✅ **Development:** Gebruik `appsettings.Local.json` voor persoonlijke credentials
2. ✅ **Production:** Gebruik Environment Variables of Secret Manager
3. ✅ **Testing:** Command-line arguments voor broker address (niet credentials!)
4. ❌ **NOOIT** commit credentials in `appsettings.json`
5. ✅ Gebruik `.gitignore` voor `appsettings.Local.json`
6. ✅ Rotate credentials regelmatig
7. ✅ Gebruik strong passwords
8. ✅ Enable authentication op MQTT broker

## Troubleshooting

### "Configuration loaded: ... (no authentication)"
- Username is leeg in configuratie
- Check `appsettings.json` en `appsettings.Local.json`
- Check environment variables

### "Warning: Could not load configuration"
- `appsettings.json` bestaat niet of is corrupt
- JSON syntax error in config file
- File permissions issue

### Credentials worden niet geladen
- Check prioriteit: Local.json > Environment > appsettings.json
- Environment variable naam correct? (gebruik `__` niet `:`)
- Restart terminal na environment variable changes

## Meer Informatie

- [SECURITY.md](SECURITY.md) - Security best practices en credential management
- [README.md](PortentaMqttClient/README.md) - Algemene documentatie
- [QUICKSTART.md](QUICKSTART.md) - Snelle start gids
