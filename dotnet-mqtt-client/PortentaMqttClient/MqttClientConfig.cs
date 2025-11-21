namespace PortentaMqttClient
{
    /// <summary>
    /// Configuration for the MQTT client connection and subscriptions
    /// </summary>
    public class MqttClientConfig
    {
        public string BrokerAddress { get; set; } = "192.168.18.74";
        public int BrokerPort { get; set; } = 1883;
        public string? Username { get; set; }
        public string? Password { get; set; }
        public string ClientIdPrefix { get; set; } = "PortentaMqttClient";
        public int ReconnectDelaySeconds { get; set; } = 5;

        // Subscription settings
        public bool SubscribeToDigitalInputs { get; set; } = true;
        public bool SubscribeToAnalogInputs { get; set; } = true;
        public bool SubscribeToProgrammableDIO { get; set; } = true;
        public bool SubscribeToSystemStatus { get; set; } = true;
        public bool SubscribeToDetailedStatus { get; set; } = true;

        // Display settings
        public bool ShowTimestamps { get; set; } = true;
        public bool ColorCodedOutput { get; set; } = true;
        public bool VerboseLogging { get; set; } = false;
    }
}
