using MQTTnet;
using MQTTnet.Client;
using System.Text;
using System.Text.Json;
using Microsoft.Extensions.Configuration;

namespace PortentaMqttClient
{
    class Program
    {
        private static IMqttClient? mqttClient;
        private static IConfiguration? configuration;
        private static string brokerAddress = "192.168.18.74";
        private static int brokerPort = 1883;
        private static string? username = null;
        private static string? password = null;

        static async Task Main(string[] args)
        {
            Console.WriteLine("=== Portenta MQTT Client ===");
            Console.WriteLine("Subscribing to digital inputs and state changes\n");

            // Load configuration from appsettings.json
            LoadConfiguration();

            // Parse command line arguments for broker address (overrides config)
            if (args.Length > 0)
            {
                brokerAddress = args[0];
            }
            if (args.Length > 1)
            {
                brokerPort = int.Parse(args[1]);
            }

            Console.WriteLine($"Connecting to MQTT Broker: {brokerAddress}:{brokerPort}");

            // Create MQTT client
            var factory = new MqttFactory();
            mqttClient = factory.CreateMqttClient();

            // Setup event handlers
            mqttClient.ApplicationMessageReceivedAsync += OnMessageReceived;
            mqttClient.DisconnectedAsync += OnDisconnected;

            // Connect to broker
            await ConnectAsync();

            // Keep running until user presses a key
            Console.WriteLine("\nPress any key to exit...\n");
            Console.ReadKey();

            // Cleanup
            await mqttClient.DisconnectAsync();
        }

        private static async Task ConnectAsync()
        {
            try
            {
                var options = new MqttClientOptionsBuilder()
                    .WithTcpServer(brokerAddress, brokerPort)
                    .WithClientId($"PortentaMqttClient_{Guid.NewGuid()}")
                    .WithCleanSession();

                // Add credentials if provided
                if (!string.IsNullOrEmpty(username) && !string.IsNullOrEmpty(password))
                {
                    options.WithCredentials(username, password);
                }

                var connectResult = await mqttClient!.ConnectAsync(options.Build());

                if (connectResult.ResultCode == MqttClientConnectResultCode.Success)
                {
                    Console.WriteLine("Connected to MQTT broker successfully!\n");
                    await SubscribeToTopics();
                }
                else
                {
                    Console.WriteLine($"Failed to connect: {connectResult.ResultCode}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Connection error: {ex.Message}");
            }
        }

        private static async Task SubscribeToTopics()
        {
            try
            {
                // Load topics from configuration
                var topics = configuration?.GetSection("MqttSettings:Topics").Get<string[]>() ?? Array.Empty<string>();

                if (topics.Length == 0)
                {
                    Console.WriteLine("Warning: No topics configured in appsettings.json");
                    Console.WriteLine("Add topics to MqttSettings:Topics array");
                    return;
                }

                // Build subscription options from configured topics
                var builder = new MqttClientSubscribeOptionsBuilder();
                foreach (var topic in topics)
                {
                    builder.WithTopicFilter(f => f.WithTopic(topic));
                }
                var subscribeOptions = builder.Build();

                var subscribeResult = await mqttClient!.SubscribeAsync(subscribeOptions);

                Console.WriteLine($"Subscribed to {topics.Length} topic(s):");
                foreach (var topic in topics)
                {
                    Console.WriteLine($"  - {topic}");
                }
                Console.WriteLine("\nListening for state changes...\n");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Subscribe error: {ex.Message}");
            }
        }

        private static Task OnMessageReceived(MqttApplicationMessageReceivedEventArgs e)
        {
            var topic = e.ApplicationMessage.Topic;
            var payload = Encoding.UTF8.GetString(e.ApplicationMessage.PayloadSegment);
            var timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss.fff");

            Console.ForegroundColor = ConsoleColor.Cyan;
            Console.Write($"[{timestamp}] ");
            Console.ResetColor();

            // Handle different message types
            if (topic.EndsWith("/state"))
            {
                // Input state change
                HandleStateChange(topic, payload);
            }
            else if (topic == "portenta/status")
            {
                // System status
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine($"System Status: {payload}");
                Console.ResetColor();
            }
            else if (topic == "portenta/status/detail")
            {
                // Detailed system status (JSON)
                HandleDetailedStatus(payload);
            }
            else
            {
                // Other messages
                Console.WriteLine($"{topic}: {payload}");
            }

            return Task.CompletedTask;
        }

        private static void HandleStateChange(string topic, string payload)
        {
            // Extract pin name from topic (e.g., "portenta/digital_in1/state" -> "digital_in1")
            var parts = topic.Split('/');
            if (parts.Length >= 2)
            {
                var pinName = parts[1];

                // Try to parse as float
                if (float.TryParse(payload, out float value))
                {
                    // Color code based on input type
                    if (pinName.StartsWith("digital_in"))
                    {
                        Console.ForegroundColor = value > 0.5 ? ConsoleColor.Green : ConsoleColor.DarkGray;
                        Console.Write($"{pinName}: ");
                        Console.Write(value > 0.5 ? "HIGH (1)" : "LOW (0)");
                        Console.ResetColor();
                        Console.WriteLine($" [{value:F3}]");
                    }
                    else if (pinName.StartsWith("analog_in"))
                    {
                        var value2 = value / 100;
                        Console.ForegroundColor = ConsoleColor.Magenta;
                        Console.Write($"{pinName}: ");
                        Console.ResetColor();
                        Console.WriteLine($"{value2:F2}V");
                    }
                    else if (pinName.StartsWith("prog_dio"))
                    {
                        Console.ForegroundColor = value > 0.5 ? ConsoleColor.Green : ConsoleColor.DarkGray;
                        Console.Write($"{pinName}: ");
                        Console.Write(value > 0.5 ? "HIGH" : "LOW");
                        Console.ResetColor();
                        Console.WriteLine($" [{value:F3}]");
                    }
                    else
                    {
                        Console.WriteLine($"{pinName}: {value:F3}");
                    }
                }
                else
                {
                    Console.WriteLine($"{pinName}: {payload}");
                }
            }
        }

        private static void HandleDetailedStatus(string jsonPayload)
        {
            try
            {
                using var doc = JsonDocument.Parse(jsonPayload);
                var root = doc.RootElement;

                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine("=== System Status Detail ===");
                Console.ResetColor();

                if (root.TryGetProperty("uptime", out var uptime))
                {
                    var uptimeMs = uptime.GetInt64();
                    var uptimeSpan = TimeSpan.FromMilliseconds(uptimeMs);
                    Console.WriteLine($"Uptime: {uptimeSpan.Days}d {uptimeSpan.Hours:D2}h {uptimeSpan.Minutes:D2}m {uptimeSpan.Seconds:D2}s");
                }

                if (root.TryGetProperty("network", out var network))
                {
                    Console.Write("Network: ");
                    Console.ForegroundColor = ConsoleColor.Green;
                    Console.WriteLine(network.GetString());
                    Console.ResetColor();
                }

                if (root.TryGetProperty("rssi", out var rssi))
                {
                    var rssiValue = rssi.GetInt32();
                    Console.Write($"WiFi Signal: {rssiValue} dBm ");
                    Console.ForegroundColor = rssiValue > -60 ? ConsoleColor.Green :
                                              rssiValue > -70 ? ConsoleColor.Yellow : ConsoleColor.Red;
                    Console.WriteLine(rssiValue > -60 ? "(Excellent)" :
                                     rssiValue > -70 ? "(Good)" : "(Poor)");
                    Console.ResetColor();
                }

                if (root.TryGetProperty("ioHealth", out var ioHealth))
                {
                    Console.Write("I/O Health: ");
                    var healthy = ioHealth.GetBoolean();
                    Console.ForegroundColor = healthy ? ConsoleColor.Green : ConsoleColor.Red;
                    Console.WriteLine(healthy ? "OK" : "ERROR");
                    Console.ResetColor();
                }

                if (root.TryGetProperty("pins", out var pins) && pins.ValueKind == JsonValueKind.Array)
                {
                    Console.WriteLine($"\nConfigured Pins: {pins.GetArrayLength()}");
                }

                Console.WriteLine();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error parsing status JSON: {ex.Message}");
            }
        }

        private static async Task OnDisconnected(MqttClientDisconnectedEventArgs e)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine($"\nDisconnected from broker: {e.Reason}");
            Console.ResetColor();

            if (e.ClientWasConnected)
            {
                // Try to reconnect after 5 seconds
                Console.WriteLine("Attempting to reconnect in 5 seconds...");
                await Task.Delay(5000);
                await ConnectAsync();
            }
        }

        private static void LoadConfiguration()
        {
            try
            {
                // Build configuration from appsettings.json and appsettings.Local.json
                configuration = new ConfigurationBuilder()
                    .SetBasePath(Directory.GetCurrentDirectory())
                    .AddJsonFile("appsettings.json", optional: false, reloadOnChange: true)
                    .AddJsonFile("appsettings.Local.json", optional: true, reloadOnChange: true)
                    .AddEnvironmentVariables()
                    .Build();

                // Load MQTT settings from configuration
                brokerAddress = configuration["MqttSettings:BrokerAddress"] ?? brokerAddress;
                brokerPort = int.TryParse(configuration["MqttSettings:BrokerPort"], out var port) ? port : brokerPort;
                username = configuration["MqttSettings:Username"];
                password = configuration["MqttSettings:Password"];

                // Don't log if username/password are empty
                if (!string.IsNullOrEmpty(username))
                {
                    Console.WriteLine($"Configuration loaded: {brokerAddress}:{brokerPort} (user: {username})");
                }
                else
                {
                    Console.WriteLine($"Configuration loaded: {brokerAddress}:{brokerPort} (no authentication)");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Warning: Could not load configuration: {ex.Message}");
                Console.WriteLine("Using default/hardcoded settings.");
            }
        }
    }
}
