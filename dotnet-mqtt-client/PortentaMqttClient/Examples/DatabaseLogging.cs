// EXAMPLE: Database logging van MQTT state changes
// Dit bestand laat zien hoe je state changes kunt loggen naar een database
//
// Om te gebruiken:
// 1. Voeg een database package toe (bijv. Entity Framework Core)
// 2. Pas OnMessageReceived aan om deze logger te gebruiken
// 3. Configureer je database connection string

using System;
using System.Collections.Generic;

namespace PortentaMqttClient.Examples
{
    /// <summary>
    /// Voorbeeld: State change log entry voor database opslag
    /// </summary>
    public class StateChangeLog
    {
        public int Id { get; set; }
        public DateTime Timestamp { get; set; }
        public string Topic { get; set; } = string.Empty;
        public string PinName { get; set; } = string.Empty;
        public float Value { get; set; }
        public string? RawPayload { get; set; }
    }

    /// <summary>
    /// Voorbeeld: Simple in-memory logger (vervang met echte database implementatie)
    /// </summary>
    public class StateChangeLogger
    {
        private readonly List<StateChangeLog> _logs = new();
        private readonly int _maxLogsInMemory = 1000;

        public void LogStateChange(string topic, string pinName, float value, string? rawPayload = null)
        {
            var log = new StateChangeLog
            {
                Timestamp = DateTime.Now,
                Topic = topic,
                PinName = pinName,
                Value = value,
                RawPayload = rawPayload
            };

            _logs.Add(log);

            // Keep memory usage under control
            if (_logs.Count > _maxLogsInMemory)
            {
                _logs.RemoveAt(0);
            }

            // TODO: Write to database here
            // await dbContext.StateChangeLogs.AddAsync(log);
            // await dbContext.SaveChangesAsync();
        }

        public IReadOnlyList<StateChangeLog> GetRecentLogs(int count = 100)
        {
            return _logs.TakeLast(count).ToList();
        }

        public IReadOnlyList<StateChangeLog> GetLogsByPin(string pinName, int count = 100)
        {
            return _logs
                .Where(l => l.PinName == pinName)
                .TakeLast(count)
                .ToList();
        }

        public void PrintStatistics()
        {
            Console.WriteLine("\n=== Logging Statistics ===");
            Console.WriteLine($"Total logs: {_logs.Count}");

            var groupedByPin = _logs.GroupBy(l => l.PinName);
            Console.WriteLine("\nLogs per pin:");
            foreach (var group in groupedByPin)
            {
                Console.WriteLine($"  {group.Key}: {group.Count()} changes");
            }

            if (_logs.Any())
            {
                var firstLog = _logs.First();
                var lastLog = _logs.Last();
                var duration = lastLog.Timestamp - firstLog.Timestamp;
                Console.WriteLine($"\nLogging duration: {duration.TotalMinutes:F1} minutes");
            }
        }
    }

    /// <summary>
    /// Voorbeeld gebruik in Program.cs:
    ///
    /// private static StateChangeLogger logger = new StateChangeLogger();
    ///
    /// private static Task OnMessageReceived(MqttApplicationMessageReceivedEventArgs e)
    /// {
    ///     var topic = e.ApplicationMessage.Topic;
    ///     var payload = Encoding.UTF8.GetString(e.ApplicationMessage.PayloadSegment);
    ///
    ///     if (topic.EndsWith("/state"))
    ///     {
    ///         var pinName = ExtractPinName(topic);
    ///         if (float.TryParse(payload, out float value))
    ///         {
    ///             logger.LogStateChange(topic, pinName, value, payload);
    ///         }
    ///     }
    ///
    ///     // ... rest of handling
    ///     return Task.CompletedTask;
    /// }
    ///
    /// // Periodiek statistics printen:
    /// Timer statsTimer = new Timer(_ => logger.PrintStatistics(), null,
    ///                               TimeSpan.FromMinutes(5),
    ///                               TimeSpan.FromMinutes(5));
    /// </summary>
}
