# Quick Start Guide - Portenta MQTT Client

## What You Need

1. .NET 6.0 SDK or higher installed
2. A running MQTT broker (for example Mosquitto)
3. Arduino Portenta Machine Control with MQTT firmware

## Step 1: Configure the Broker

Open `PortentaMqttClient/Program.cs` and adjust the broker settings (lines 11-14):

```csharp
private static string brokerAddress = "192.168.1.100"; // Replace with your broker IP
private static int brokerPort = 1883;
```

## Step 2: Build the Application

```bash
cd dotnet-mqtt-client/PortentaMqttClient
dotnet build
```

## Step 3: Run the Client

```bash
dotnet run
```

Or with custom broker settings:

```bash
dotnet run 192.168.1.100 1883
```

## What Happens?

The client:
1. Connects to the MQTT broker
2. Subscribes to all Portenta input topics
3. Displays real-time updates of:
   - Digital inputs (8 channels)
   - Analog voltage inputs (3 channels, 0-10V)
   - Programmable DIO (12 channels)
   - System status

## Expected Output

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
```

## Troubleshooting

### "Connection refused"
- Check if your MQTT broker is running
- Verify firewall settings
- Ping the broker: `ping 192.168.1.100`

### "No messages received"
- Test with mosquitto_sub: `mosquitto_sub -h 192.168.1.100 -t "portenta/#" -v`
- Check if the Portenta controller is online
- Check the Portenta serial monitor for errors

### Port already in use
- Make sure you're not running other MQTT clients on the same machine
- The client creates a unique client ID, so this shouldn't normally happen

## Next Steps

1. **Add Logging**: Write state changes to a database or file
2. **Create UI**: Build a WPF/Avalonia GUI for visual monitoring
3. **Set Alerts**: Send notifications on specific conditions
4. **Add Control**: Implement relay/output control (publish to `/set` topics)

## Add Relay Control

If you also want to control outputs, add this function to Program.cs:

```csharp
public static async Task SetRelayState(int relayNumber, bool state)
{
    var message = new MqttApplicationMessageBuilder()
        .WithTopic($"portenta/relay{relayNumber}/set")
        .WithPayload(state ? "1" : "0")
        .Build();

    await mqttClient.PublishAsync(message);
    Console.WriteLine($"Sent: relay{relayNumber} = {(state ? "ON" : "OFF")}");
}
```

Usage:
```csharp
await SetRelayState(0, true);  // Turn relay 0 ON
await SetRelayState(0, false); // Turn relay 0 OFF
```

## More Information

See [README.md](PortentaMqttClient/README.md) for detailed documentation.
