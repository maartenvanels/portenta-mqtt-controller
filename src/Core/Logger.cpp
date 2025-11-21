#include "Core/Logger.h"
#include "Core/TimeManager.h"
#include <time.h>

namespace Logging
{

    Logger &Logger::getInstance()
    {
        static Logger instance;
        return instance;
    }

    Logger::Logger()
        : storage_(nullptr), initialized_(false), minLevel_(LogLevel::INFO), maxLogSize_(500 * 1024) // 500 KB default
          ,
          lastFlush_(0)
    {
        logBuffer_.reserve(BUFFER_SIZE);
    }

    bool Logger::initialize(size_t maxLogSizeKB)
    {
        if (initialized_)
        {
            return true;
        }

        maxLogSize_ = maxLogSizeKB * 1024;

        Serial.println("\n=== Initializing Logger ===");
        Serial.print("Max log size: ");
        Serial.print(maxLogSizeKB);
        Serial.println(" KB");

        // Use Partition 4 (User Data) - InternalStorage(3)
        storage_ = new InternalStorage(3, "user", FS_LITTLEFS);

        if (!storage_->begin())
        {
            Serial.println("Warning: User partition not formatted, attempting format...");

            if (!storage_->format(FS_LITTLEFS))
            {
                Serial.println("ERROR: Failed to format user partition for logs");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
                delete storage_;
#pragma GCC diagnostic pop
                storage_ = nullptr;
                return false;
            }

            if (!storage_->begin())
            {
                Serial.println("ERROR: Failed to initialize user partition after format");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
                delete storage_;
#pragma GCC diagnostic pop
                storage_ = nullptr;
                return false;
            }
        }

        Serial.println("Logger initialized successfully");
        Serial.println("Logs will be stored in QSPI Partition 4 (User Data)");

        initialized_ = true;

        // Log system startup
        info("=== SYSTEM BOOT ===");
        info("Logger initialized, logs persistent across firmware updates");

        return true;
    }

    void Logger::debug(const String &message)
    {
        log(LogLevel::DEBUG, message);
    }

    void Logger::info(const String &message)
    {
        log(LogLevel::INFO, message);
    }

    void Logger::warning(const String &message)
    {
        log(LogLevel::WARNING, message);
    }

    void Logger::error(const String &message)
    {
        log(LogLevel::ERROR, message);
    }

    void Logger::critical(const String &message)
    {
        log(LogLevel::CRITICAL, message);
    }

    void Logger::log(LogLevel level, const String &message)
    {
        // Check if level is high enough
        if (level < minLevel_)
        {
            return;
        }

        // Always print to Serial (Serial is thread-safe in mbed)
        Serial.print("[");
        Serial.print(logLevelToString(level));
        Serial.print("] ");
        Serial.println(message);

        if (!initialized_)
        {
            return; // Can't write to QSPI yet
        }

        mutex_.lock();
        writeLogEntry(level, message);
        mutex_.unlock();
    }

    void Logger::writeLogEntry(LogLevel level, const String &message)
    {
        // Assumes mutex is locked by caller (log())

        String timestamp;
        if (TimeManager::getInstance().isSynchronized())
        {
            timestamp = TimeManager::getInstance().getFormattedTime();
        }
        else
        {
            timestamp = String(millis());
        }

        String entry = formatLogEntry(level, message, timestamp);

        // Add to buffer
        logBuffer_.push_back(entry);

        // Auto-flush if buffer full or enough time passed
        if (logBuffer_.size() >= BUFFER_SIZE || (millis() - lastFlush_) > 5000)
        {
            // Internal flush call - don't lock mutex again as it's not recursive
            if (!logBuffer_.empty())
            {
                rotateLogsIfNeeded();

                auto root = storage_->getRootFolder();
                auto file = root.createFile("system.log", FileMode::APPEND);

                if (!file.exists())
                {
                    Serial.println("ERROR: Failed to open log file");
                    logBuffer_.clear();
                    return;
                }

                // Write all buffered entries
                for (const String &entry : logBuffer_)
                {
                    file.write((const uint8_t *)entry.c_str(), entry.length());
                }

                file.close();

                size_t count = logBuffer_.size();
                logBuffer_.clear();
                lastFlush_ = millis();
            }
        }
    }

    String Logger::formatLogEntry(LogLevel level, const String &message, const String &timestamp)
    {
        // Format: [TIMESTAMP][LEVEL] Message
        String entry;
        entry.reserve(message.length() + 50);

        entry += "[";
        entry += timestamp;
        entry += "][";
        entry += logLevelToString(level);
        entry += "] ";
        entry += message;
        entry += "\n";

        return entry;
    }

    void Logger::flush()
    {
        mutex_.lock();
        if (!initialized_ || logBuffer_.empty())
        {
            mutex_.unlock();
            return;
        }

        // Check if rotation needed before writing
        rotateLogsIfNeeded();

        auto root = storage_->getRootFolder();
        auto file = root.createFile("system.log", FileMode::APPEND);

        if (!file.exists())
        {
            Serial.println("ERROR: Failed to open log file");
            logBuffer_.clear();
            mutex_.unlock();
            return;
        }

        // Write all buffered entries
        for (const String &entry : logBuffer_)
        {
            file.write((const uint8_t *)entry.c_str(), entry.length());
        }

        file.close();

        size_t count = logBuffer_.size();
        logBuffer_.clear();
        lastFlush_ = millis();

        mutex_.unlock();

        // Don't log about flushing to avoid recursion
        if (count > 0)
        {
            Serial.print("Flushed ");
            Serial.print(count);
            Serial.println(" log entries to QSPI");
        }
    }

    void Logger::rotateLogsIfNeeded()
    {
        auto root = storage_->getRootFolder();
        auto file = root.createFile("system.log", FileMode::READ);

        if (!file.exists())
        {
            return; // No log file yet
        }

        size_t currentSize = file.available();
        file.close();

        if (currentSize < maxLogSize_)
        {
            return; // No rotation needed
        }

        Serial.println("\n=== Log Rotation ===");
        Serial.print("Current log size: ");
        Serial.print(currentSize / 1024);
        Serial.println(" KB");

        // Rotate: system.log -> system.log.old
        // Delete old backup if exists
        auto oldLog = root.createFile("system.log.old", FileMode::READ);
        if (oldLog.exists())
        {
            oldLog.close();
            oldLog.remove(); // Use file.remove() not root.remove()
            Serial.println("Removed old backup log");
        }

        // Copy current to .old (UnifiedStorage doesn't have rename)
        file = root.createFile("system.log", FileMode::READ);
        auto newOld = root.createFile("system.log.old", FileMode::WRITE);

        // Copy in chunks
        const size_t chunkSize = 512;
        uint8_t buffer[chunkSize];

        while (file.available())
        {
            size_t toRead = (file.available() < chunkSize) ? file.available() : chunkSize;
            file.read(buffer, toRead);
            newOld.write(buffer, toRead);
        }

        file.close();
        newOld.close();
        Serial.println("Copied system.log -> system.log.old");

        // Delete current log
        file = root.createFile("system.log", FileMode::READ);
        file.remove();
        Serial.println("Removed system.log");

        // New log file will be created on next write
        Serial.println("New log file will be created");
    }

    String Logger::readLogs(size_t maxLines)
    {
        if (!initialized_)
        {
            return "Logger not initialized";
        }

        // Flush any buffered logs first
        flush();

        auto root = storage_->getRootFolder();
        auto file = root.createFile("system.log", FileMode::READ);

        if (!file.exists())
        {
            return "No logs available";
        }

        // Optimization: Read only the end of the file if it's large
        size_t fileSize = file.available();
        size_t bytesToRead = fileSize;
        const size_t AVG_LINE_LEN = 100; // Conservative estimate

        // Calculate bytes needed for requested lines, but cap at 32KB (half of max allocatable usually)
        size_t targetBytes = maxLines * AVG_LINE_LEN;
        if (targetBytes > 32768)
            targetBytes = 32768;

        if (fileSize > targetBytes)
        {
            bytesToRead = targetBytes;
            file.seek(fileSize - bytesToRead);
        }

        String logs;
        // Pre-allocate carefully to avoid fragmentation
        if (!logs.reserve(bytesToRead + 128))
        {
            // If allocation fails, try a smaller amount
            bytesToRead /= 2;
            file.seek(fileSize - bytesToRead);
            if (!logs.reserve(bytesToRead + 128))
            {
                file.close();
                return "Error: Out of memory while reading logs";
            }
        }

        std::vector<String> lines;
        // lines.reserve(maxLines); // Avoid vector pre-allocation to save RAM

        String currentLine;
        currentLine.reserve(256);

        // If we started in the middle of a file, we might be in the middle of a line.
        // We'll discard the first partial line if we seeked.
        bool firstLine = (fileSize > bytesToRead);

        while (file.available())
        {
            char c = (char)file.read();
            if (c == '\n')
            {
                if (firstLine)
                {
                    firstLine = false;
                    currentLine = ""; // Discard potential partial line
                }
                else if (currentLine.length() > 0)
                {
                    lines.push_back(currentLine);
                    currentLine = "";
                }
            }
            else
            {
                // Limit line length to prevent memory issues with corrupt logs
                if (currentLine.length() < 1024)
                {
                    currentLine += c;
                }
            }
        }

        // Add last line if not empty and not partial
        if (currentLine.length() > 0 && !firstLine)
        {
            lines.push_back(currentLine);
        }

        file.close();

        // Return last N lines
        size_t startIdx = (lines.size() > maxLines) ? (lines.size() - maxLines) : 0;

        logs = ""; // Clear any partial data
        for (size_t i = startIdx; i < lines.size(); i++)
        {
            logs += lines[i];
            logs += "\n";
        }

        return logs;
    }

    String Logger::readLogsSince(uint32_t timestamp)
    {
        if (!initialized_)
        {
            return "Logger not initialized";
        }

        flush();

        auto root = storage_->getRootFolder();
        auto file = root.createFile("system.log", FileMode::READ);

        if (!file.exists())
        {
            return "No logs available";
        }

        String logs;
        logs.reserve(4096);

        String currentLine;

        while (file.available())
        {
            char c = (char)file.read();
            if (c == '\n')
            {
                if (!currentLine.isEmpty())
                {
                    // Parse timestamp from line: [TIMESTAMP][LEVEL] Message
                    int firstBracket = currentLine.indexOf('[');
                    int secondBracket = currentLine.indexOf(']', firstBracket + 1);

                    if (firstBracket != -1 && secondBracket != -1)
                    {
                        String timestampStr = currentLine.substring(firstBracket + 1, secondBracket);
                        uint32_t logTimestamp = 0;

                        if (timestampStr.indexOf('-') != -1)
                        {
                            // Parse date string to timestamp (YYYY-MM-DD HH:MM:SS)
                            int y, m, d, h, min, s;
                            if (sscanf(timestampStr.c_str(), "%d-%d-%d %d:%d:%d", &y, &m, &d, &h, &min, &s) == 6)
                            {
                                struct tm tm = {0};
                                tm.tm_year = y - 1900;
                                tm.tm_mon = m - 1;
                                tm.tm_mday = d;
                                tm.tm_hour = h;
                                tm.tm_min = min;
                                tm.tm_sec = s;
                                logTimestamp = (uint32_t)mktime(&tm);
                            }
                        }
                        else
                        {
                            logTimestamp = timestampStr.toInt();
                        }

                        if (logTimestamp >= timestamp)
                        {
                            logs += currentLine;
                            logs += "\n";
                        }
                    }
                    currentLine = "";
                }
            }
            else
            {
                currentLine += c;
            }
        }

        file.close();
        return logs;
    }

    void Logger::clearLogs()
    {
        if (!initialized_)
        {
            return;
        }

        Serial.println("\n=== Clearing Logs ===");

        mutex_.lock();

        auto root = storage_->getRootFolder();

        // Remove current log
        auto file = root.createFile("system.log", FileMode::READ);
        if (file.exists())
        {
            file.remove(); // Use file.remove() not root.remove()
            Serial.println("Removed system.log");
        }

        // Remove backup log
        file = root.createFile("system.log.old", FileMode::READ);
        if (file.exists())
        {
            file.remove(); // Use file.remove() not root.remove()
            Serial.println("Removed system.log.old");
        }

        // Clear buffer
        logBuffer_.clear();

        mutex_.unlock();

        info("Logs cleared");
    }

    void Logger::setLogLevel(LogLevel minLevel)
    {
        minLevel_ = minLevel;
        Serial.print("Log level set to: ");
        Serial.println(logLevelToString(minLevel_));
    }

    size_t Logger::getLogFileSize()
    {
        if (!initialized_)
        {
            return 0;
        }

        auto root = storage_->getRootFolder();
        auto file = root.createFile("system.log", FileMode::READ);

        if (!file.exists())
        {
            return 0;
        }

        size_t size = file.available();
        file.close();
        return size;
    }

    size_t Logger::getLogCount()
    {
        if (!initialized_)
        {
            return 0;
        }

        auto root = storage_->getRootFolder();
        auto file = root.createFile("system.log", FileMode::READ);

        if (!file.exists())
        {
            return 0;
        }

        size_t count = 0;
        while (file.available())
        {
            char c = (char)file.read();
            if (c == '\n')
            {
                count++;
            }
        }

        file.close();
        return count;
    }

    const char *Logger::logLevelToString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARN";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::CRITICAL:
            return "CRIT";
        default:
            return "UNKNOWN";
        }
    }

} // namespace Logging
