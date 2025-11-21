#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <Arduino_UnifiedStorage.h>
#include <vector>
#include <rtos.h>

namespace Logging
{

    enum class LogLevel
    {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        CRITICAL = 4
    };

    class Logger
    {
    public:
        static Logger &getInstance();

        // Initialize logger with QSPI User partition
        bool initialize(size_t maxLogSizeKB = 500); // Max 500KB per log file

        // Log methods
        void debug(const String &message);
        void info(const String &message);
        void warning(const String &message);
        void error(const String &message);
        void critical(const String &message);

        // Generic log with level
        void log(LogLevel level, const String &message);

        // Flush buffered logs to QSPI
        void flush();

        // Read logs from QSPI
        String readLogs(size_t maxLines = 100);
        String readLogsSince(uint32_t timestamp); // Read logs since timestamp

        // Clear all logs
        void clearLogs();

        // Set minimum log level (logs below this won't be written)
        void setLogLevel(LogLevel minLevel);

        // Get statistics
        size_t getLogFileSize();
        size_t getLogCount();

    private:
        Logger();
        ~Logger() = default;

        // Prevent copying
        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;

        void writeLogEntry(LogLevel level, const String &message);
        void rotateLogsIfNeeded();
        String formatLogEntry(LogLevel level, const String &message, const String &timestamp);
        const char *logLevelToString(LogLevel level);

        InternalStorage *storage_;
        bool initialized_;
        LogLevel minLevel_;
        size_t maxLogSize_;

        // Buffering for performance
        std::vector<String> logBuffer_;
        static const size_t BUFFER_SIZE = 10; // Flush every 10 entries
        uint32_t lastFlush_;

        rtos::Mutex mutex_; // Protects access to logBuffer_ and file operations
    };

// Convenience macros
#define LOG_DEBUG(msg) Logging::Logger::getInstance().debug(msg)
#define LOG_INFO(msg) Logging::Logger::getInstance().info(msg)
#define LOG_WARNING(msg) Logging::Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) Logging::Logger::getInstance().error(msg)
#define LOG_CRITICAL(msg) Logging::Logger::getInstance().critical(msg)

} // namespace Logging

#endif // LOGGER_H
