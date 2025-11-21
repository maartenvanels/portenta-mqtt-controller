// EXAMPLE: Control van Portenta outputs via MQTT
// Dit bestand laat zien hoe je relays en andere outputs kunt aansturen
//
// Om te gebruiken:
// 1. Voeg deze class toe aan je project
// 2. Gebruik de PortentaController in je Main() of andere methods

using MQTTnet;
using MQTTnet.Client;
using System;
using System.Threading.Tasks;

namespace PortentaMqttClient.Examples
{
    /// <summary>
    /// Controller voor het aansturen van Portenta outputs
    /// </summary>
    public class PortentaController
    {
        private readonly IMqttClient _mqttClient;
        private readonly string _topicPrefix;

        public PortentaController(IMqttClient mqttClient, string topicPrefix = "portenta")
        {
            _mqttClient = mqttClient ?? throw new ArgumentNullException(nameof(mqttClient));
            _topicPrefix = topicPrefix;
        }

        #region Digital Outputs (Relays)

        /// <summary>
        /// Schakel een relay aan of uit
        /// </summary>
        /// <param name="relayNumber">Relay nummer (0-7)</param>
        /// <param name="state">true = ON, false = OFF</param>
        public async Task SetRelayAsync(int relayNumber, bool state)
        {
            ValidateRelayNumber(relayNumber);
            await PublishAsync($"{_topicPrefix}/relay{relayNumber}/set", state ? "1" : "0");
            Console.WriteLine($"Relay {relayNumber} set to {(state ? "ON" : "OFF")}");
        }

        /// <summary>
        /// Schakel alle relays tegelijk aan of uit
        /// </summary>
        public async Task SetAllRelaysAsync(bool state)
        {
            for (int i = 0; i < 8; i++)
            {
                await SetRelayAsync(i, state);
                await Task.Delay(50); // Kleine delay tussen commands
            }
        }

        /// <summary>
        /// Toggle een relay (wissel tussen ON en OFF)
        /// Voor deze functie moet je eerst de huidige state weten via subscriptie
        /// </summary>
        public async Task ToggleRelayAsync(int relayNumber, bool currentState)
        {
            await SetRelayAsync(relayNumber, !currentState);
        }

        #endregion

        #region Programmable DIO Outputs

        /// <summary>
        /// Schakel een programmable DIO output aan of uit
        /// </summary>
        /// <param name="dioNumber">DIO nummer (0-11, maar 6-11 zijn typisch outputs)</param>
        /// <param name="state">true = HIGH, false = LOW</param>
        public async Task SetProgrammableDIOAsync(int dioNumber, bool state)
        {
            ValidateDIONumber(dioNumber);
            await PublishAsync($"{_topicPrefix}/prog_dio/{dioNumber}/set", state ? "1" : "0");
            Console.WriteLine($"Programmable DIO {dioNumber} set to {(state ? "HIGH" : "LOW")}");
        }

        #endregion

        #region Analog Outputs

        /// <summary>
        /// Stel een analoge voltage output in (0-10V)
        /// </summary>
        /// <param name="outputNumber">Analog output nummer (0-3)</param>
        /// <param name="voltage">Voltage (0.0 - 10.0V)</param>
        public async Task SetAnalogOutputAsync(int outputNumber, float voltage)
        {
            ValidateAnalogOutputNumber(outputNumber);
            ValidateVoltage(voltage);

            await PublishAsync($"{_topicPrefix}/analog_out{outputNumber}/set", voltage.ToString("F3"));
            Console.WriteLine($"Analog output {outputNumber} set to {voltage:F3}V");
        }

        /// <summary>
        /// Stel analoge output in als percentage (0-100%)
        /// </summary>
        public async Task SetAnalogOutputPercentageAsync(int outputNumber, float percentage)
        {
            ValidatePercentage(percentage);
            float voltage = (percentage / 100.0f) * 10.0f;
            await SetAnalogOutputAsync(outputNumber, voltage);
        }

        #endregion

        #region Helper Methods

        private async Task PublishAsync(string topic, string payload)
        {
            if (!_mqttClient.IsConnected)
            {
                throw new InvalidOperationException("MQTT client is not connected");
            }

            var message = new MqttApplicationMessageBuilder()
                .WithTopic(topic)
                .WithPayload(payload)
                .WithRetainFlag(false) // Don't retain control messages
                .Build();

            await _mqttClient.PublishAsync(message);
        }

        private static void ValidateRelayNumber(int relayNumber)
        {
            if (relayNumber < 0 || relayNumber > 7)
                throw new ArgumentOutOfRangeException(nameof(relayNumber), "Relay number must be between 0 and 7");
        }

        private static void ValidateDIONumber(int dioNumber)
        {
            if (dioNumber < 0 || dioNumber > 11)
                throw new ArgumentOutOfRangeException(nameof(dioNumber), "DIO number must be between 0 and 11");
        }

        private static void ValidateAnalogOutputNumber(int outputNumber)
        {
            if (outputNumber < 0 || outputNumber > 3)
                throw new ArgumentOutOfRangeException(nameof(outputNumber), "Analog output number must be between 0 and 3");
        }

        private static void ValidateVoltage(float voltage)
        {
            if (voltage < 0.0f || voltage > 10.0f)
                throw new ArgumentOutOfRangeException(nameof(voltage), "Voltage must be between 0.0 and 10.0V");
        }

        private static void ValidatePercentage(float percentage)
        {
            if (percentage < 0.0f || percentage > 100.0f)
                throw new ArgumentOutOfRangeException(nameof(percentage), "Percentage must be between 0 and 100");
        }

        #endregion
    }

    /// <summary>
    /// Voorbeelden van gebruik in Main():
    ///
    /// // Maak controller na MQTT connectie
    /// var controller = new PortentaController(mqttClient);
    ///
    /// // Voorbeelden:
    ///
    /// // 1. Schakel relay 0 aan
    /// await controller.SetRelayAsync(0, true);
    ///
    /// // 2. Schakel relay 0 uit na 5 seconden
    /// await Task.Delay(5000);
    /// await controller.SetRelayAsync(0, false);
    ///
    /// // 3. Schakel alle relays aan
    /// await controller.SetAllRelaysAsync(true);
    ///
    /// // 4. Stel analoge output in op 5V
    /// await controller.SetAnalogOutputAsync(0, 5.0f);
    ///
    /// // 5. Stel analoge output in op 75%
    /// await controller.SetAnalogOutputPercentageAsync(0, 75.0f);
    ///
    /// // 6. Blink pattern op relay 0
    /// for (int i = 0; i < 5; i++)
    /// {
    ///     await controller.SetRelayAsync(0, true);
    ///     await Task.Delay(500);
    ///     await controller.SetRelayAsync(0, false);
    ///     await Task.Delay(500);
    /// }
    ///
    /// // 7. Ramp analog output van 0 naar 10V
    /// for (float v = 0; v <= 10.0f; v += 0.5f)
    /// {
    ///     await controller.SetAnalogOutputAsync(0, v);
    ///     await Task.Delay(100);
    /// }
    /// </summary>

    /// <summary>
    /// Voorbeeld: Interactieve console control
    /// Voeg dit toe aan Main() voor interactieve control:
    ///
    /// var controller = new PortentaController(mqttClient);
    ///
    /// while (true)
    /// {
    ///     Console.WriteLine("\nCommands:");
    ///     Console.WriteLine("  r0-7 on/off  - Control relay (e.g., 'r0 on')");
    ///     Console.WriteLine("  a0-3 voltage - Set analog output (e.g., 'a0 5.5')");
    ///     Console.WriteLine("  all on/off   - Control all relays");
    ///     Console.WriteLine("  quit         - Exit");
    ///     Console.Write("> ");
    ///
    ///     var input = Console.ReadLine()?.Trim().ToLower();
    ///     if (string.IsNullOrEmpty(input)) continue;
    ///
    ///     var parts = input.Split(' ');
    ///     try
    ///     {
    ///         if (input == "quit") break;
    ///
    ///         if (parts[0].StartsWith("r") && parts.Length == 2)
    ///         {
    ///             int relay = int.Parse(parts[0].Substring(1));
    ///             bool state = parts[1] == "on";
    ///             await controller.SetRelayAsync(relay, state);
    ///         }
    ///         else if (parts[0].StartsWith("a") && parts.Length == 2)
    ///         {
    ///             int output = int.Parse(parts[0].Substring(1));
    ///             float voltage = float.Parse(parts[1]);
    ///             await controller.SetAnalogOutputAsync(output, voltage);
    ///         }
    ///         else if (parts[0] == "all" && parts.Length == 2)
    ///         {
    ///             bool state = parts[1] == "on";
    ///             await controller.SetAllRelaysAsync(state);
    ///         }
    ///     }
    ///     catch (Exception ex)
    ///     {
    ///         Console.WriteLine($"Error: {ex.Message}");
    ///     }
    /// }
    /// </summary>
}
