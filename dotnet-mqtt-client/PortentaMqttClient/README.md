# Portenta MQTT Client

Een .NET console applicatie om MQTT events te ontvangen van de Arduino Portenta Machine Control.

## Functionaliteit

Deze client subscribet op verschillende MQTT topics en toont:
- Digital input state changes (HIGH/LOW)
- Analog input voltage readings
- Programmable DIO state changes
- System status updates
- Gedetailleerde status informatie

## MQTT Topics

De client luistert naar de volgende topics:

### State Topics (Inputs)
- `portenta/digital_in#/state` - Digital input states (8 channels)
- `portenta/analog_in#/state` - Analog voltage inputs (3 channels, 0-10V)
- `portenta/prog_dio/#/state` - Programmable DIO states (12 channels)

### Status Topics
- `portenta/status` - Online/offline status
- `portenta/status/detail` - Gedetailleerde JSON status (uptime, network, I/O health, pin overzicht)

## Configuratie

De client laadt configuratie uit meerdere bronnen (in prioriteit volgorde):

### 1. appsettings.json (Basis configuratie)
```json
{
  "MqttSettings": {
    "BrokerAddress": "192.168.1.100",
    "BrokerPort": 1883,
    "Username": "your-username",
    "Password": "your-password"
  }
}
```

### 2. appsettings.Local.json (Lokale overrides - AANBEVOLEN)
Maak een `appsettings.Local.json` voor persoonlijke credentials:
```bash
cp appsettings.Local.json.template appsettings.Local.json
# Edit appsettings.Local.json met jouw credentials
```

Dit bestand staat in `.gitignore` en wordt NIET gecommit.

### 3. Environment Variables (Production)
```bash
export MqttSettings__BrokerAddress="192.168.18.74"
export MqttSettings__Username="homeassistant"
export MqttSettings__Password="uw-password"
dotnet run
```

### 4. Command-line arguments (Hoogste prioriteit)
```bash
dotnet run 192.168.1.100 1883
```

**Security:** Zie [../SECURITY.md](../SECURITY.md) voor best practices!

## Gebruik

### Build en Run

```bash
cd dotnet-mqtt-client/PortentaMqttClient
dotnet build
dotnet run
```

### Met command-line argumenten

Je kunt de broker address en port ook via command-line opgeven:

```bash
dotnet run 192.168.1.100 1883
```

## Voorbeelden

### Digital Input State Change

Wanneer een digital input verandert, zie je:

```
[2025-10-29 14:23:45.123] digital_in1: HIGH (1) [1.000]
[2025-10-29 14:23:46.234] digital_in1: LOW (0) [0.000]
```

### Analog Input Reading

```
[2025-10-29 14:23:47.345] analog_in0: 5.234V
[2025-10-29 14:23:48.456] analog_in1: 7.891V
```

### System Status

```
[2025-10-29 14:23:50.000] System Status: online

=== System Status Detail ===
Uptime: 0d 02h 15m 34s
Network: wifi
WiFi Signal: -45 dBm (Excellent)
I/O Health: OK

Configured Pins: 35
```

## Hoe werkt het?

De Portenta controller publiceert automatisch MQTT messages wanneer:
1. Een input state verandert (digital of analog)
2. Periodiek elke 5 seconden (alle pin states)
3. Periodiek elke 10 seconden (system status)

De .NET client:
1. Maakt verbinding met de MQTT broker
2. Subscribet op alle relevante topics
3. Toont elke state change met timestamp en kleurcodering
4. Probeert automatisch te reconnecten bij verbindingsverlies

## Output Kleurcodering

- **Cyaan**: Timestamps
- **Groen**: HIGH/active states
- **Grijs**: LOW/inactive states
- **Magenta**: Analog voltage readings
- **Geel**: System status updates
- **Rood**: Errors en disconnections

## Vereisten

- .NET 6.0 of hoger
- MQTTnet 4.3.7 of hoger
- Toegang tot MQTT broker op je netwerk

## Uitbreidingsmogelijkheden

Je kunt de client uitbreiden met:
- Database logging van state changes
- Grafische UI (WPF/WinForms/Avalonia)
- Web API om states te exposen
- E-mail/SMS alerts bij bepaalde condities
- Data aggregatie en analytics
- Relay control (publish naar `/set` topics)

## Relay Control Voorbeeld

Om een relay aan te sturen (publish), kun je bijvoorbeeld deze code toevoegen:

```csharp
// Voorbeeld: Turn relay 0 ON
var message = new MqttApplicationMessageBuilder()
    .WithTopic("portenta/relay0/set")
    .WithPayload("1")
    .Build();

await mqttClient.PublishAsync(message);
```

## Troubleshooting

### Kan niet verbinden met broker
- Controleer of de broker IP address correct is
- Controleer of de broker draait (bijvoorbeeld Mosquitto)
- Check firewall instellingen

### Geen messages ontvangen
- Controleer of de Portenta controller online is
- Gebruik `mosquitto_sub` om te testen: `mosquitto_sub -h <broker-ip> -t "portenta/#" -v`
- Check of de Portenta correct verbonden is met de broker

## Licentie

Dit is een side project voor de Portenta MQTT Controller.
