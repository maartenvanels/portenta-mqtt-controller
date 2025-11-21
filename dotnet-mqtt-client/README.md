# .NET MQTT Client voor Portenta Machine Control

Dit is een side project: een .NET console applicatie die MQTT events ontvangt van de Arduino Portenta Machine Control.

## Projectstructuur

```
dotnet-mqtt-client/
├── PortentaMqttClient/
│   ├── Program.cs              # Hoofd applicatie - MQTT client met subscriptions
│   ├── MqttClientConfig.cs     # Configuratie class
│   ├── appsettings.json        # Configuratie bestand
│   ├── README.md               # Gedetailleerde documentatie
│   └── Examples/
│       ├── DatabaseLogging.cs  # Voorbeeld: state changes loggen naar database
│       └── OutputControl.cs    # Voorbeeld: outputs aansturen (relays, analog)
├── QUICKSTART.md               # Quick start gids
└── README.md                   # Dit bestand
```

## Wat doet deze applicatie?

De .NET MQTT client:
- Subscribet op MQTT topics van de Portenta controller
- Ontvangt real-time updates wanneer inputs veranderen
- Toont state changes met kleurcodering en timestamps
- Automatische reconnect bij verbindingsverlies
- Ondersteunt alle I/O types:
  - Digital inputs (8 channels)
  - Analog voltage inputs (3 channels, 0-10V)
  - Programmable DIO (12 channels)
  - System status updates

## Quick Start

1. **Configureer broker IP** in `PortentaMqttClient/appsettings.json`:
   ```json
   {
     "MqttSettings": {
       "BrokerAddress": "192.168.1.100"
     }
   }
   ```

2. **Build en run**:
   ```bash
   cd dotnet-mqtt-client/PortentaMqttClient
   dotnet build
   dotnet run
   ```

Zie [QUICKSTART.md](QUICKSTART.md) voor meer details.

## Features

- **Real-time monitoring**: Ontvang direct events wanneer input states veranderen
- **Kleurcodering**: Visual feedback met verschillende kleuren per input type
- **Timestamps**: Elke state change met milliseconde precisie
- **Auto-reconnect**: Automatisch opnieuw verbinden bij verbindingsverlies
- **Command-line args**: Broker address en port via argumenten instellen
- **Extensible**: Makkelijk uit te breiden met logging, UI, alerts, etc.

## MQTT Topics

De client luistert naar:

| Topic Pattern | Beschrijving |
|--------------|--------------|
| `portenta/digital_in#/state` | Digital input states (8 channels) |
| `portenta/analog_in#/state` | Analog voltage inputs (0-10V, 3 channels) |
| `portenta/prog_dio/#/state` | Programmable DIO states (12 channels) |
| `portenta/status` | System online/offline status |
| `portenta/status/detail` | Gedetailleerde JSON status info |

## Output Voorbeeld

```
=== Portenta MQTT Client ===
Subscribing to digital inputs and state changes

Connecting to MQTT Broker: 192.168.1.100:1883
Connected to MQTT broker successfully!

Subscribed to topics:
  - portenta/digital_in#/state
  - portenta/analog_in#/state
  - portenta/prog_dio/#/state
  - portenta/status
  - portenta/status/detail

Listening for state changes...

Press any key to exit...

[2025-10-29 14:23:45.123] System Status: online
[2025-10-29 14:23:46.234] digital_in1: HIGH (1) [1.000]
[2025-10-29 14:23:47.345] analog_in0: 5.234V
[2025-10-29 14:23:48.456] digital_in1: LOW (0) [0.000]

=== System Status Detail ===
Uptime: 0d 02h 15m 34s
Network: wifi
WiFi Signal: -45 dBm (Excellent)
I/O Health: OK

Configured Pins: 35
```

## Uitbreidingen

De `Examples/` directory bevat voorbeelden voor:

### 1. Database Logging (`DatabaseLogging.cs`)
Log alle state changes naar een database voor analyse en historische data.

### 2. Output Control (`OutputControl.cs`)
Stuur Portenta outputs aan vanuit .NET:
```csharp
var controller = new PortentaController(mqttClient);
await controller.SetRelayAsync(0, true);           // Relay aan
await controller.SetAnalogOutputAsync(0, 5.0f);   // 5V output
```

Mogelijke uitbreidingen:
- WPF/Avalonia GUI voor visuele monitoring
- Web API om states te exposen
- SignalR voor real-time web updates
- E-mail/SMS alerts bij bepaalde condities
- Data aggregatie en analytics
- Grafische charts van historische data

## Vereisten

- .NET 6.0 SDK of hoger
- MQTTnet 4.3.7 of hoger (automatisch geinstalleerd via NuGet)
- Toegang tot een MQTT broker
- Portenta Machine Control met MQTT firmware

## Architectuur

```
┌─────────────────────┐
│  Portenta Machine   │
│     Control         │
│  (Arduino/C++)      │
└──────────┬──────────┘
           │ MQTT
           │ Publishes state changes
           │
    ┌──────▼──────┐
    │ MQTT Broker │
    │ (Mosquitto) │
    └──────┬──────┘
           │
           │ Subscribes to topics
           │
    ┌──────▼───────────┐
    │  .NET MQTT Client│
    │  (Deze app)      │
    └──────────────────┘
```

## Integratie met Hoofdproject

Deze .NET client is een side project en is volledig onafhankelijk van de Arduino firmware.
Het communiceert alleen via MQTT met de Portenta controller.

## Licentie

Side project voor de Portenta MQTT Controller.

## Support

Voor vragen of problemen, zie:
- [PortentaMqttClient/README.md](PortentaMqttClient/README.md) - Gedetailleerde documentatie
- [QUICKSTART.md](QUICKSTART.md) - Snelle start gids
- Hoofdproject documentation in parent directory
