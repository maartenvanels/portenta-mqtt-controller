#include "Network/WebServer.h"
#include "Network/WebAssets.h"
#include "Core/credentials.h"
#include "Network/NetworkSettings.h"
#include "Network/NetworkManager.h"
#include "Core/TimeManager.h"
#include "Services/Ota/OTAUpdate.h"
#include "Core/Logger.h"
#include "Core/SystemDiagnostics.h"
#include <ArduinoJson.h>
#include <mbed_stats.h>

namespace Web
{

// Type aliases for network server based on configuration
#ifdef USE_ETHERNET
    typedef EthernetServer NetworkServer;
#else
    typedef WiFiServer NetworkServer;
#endif

    WebServer &WebServer::getInstance()
    {
        static WebServer instance;
        return instance;
    }

    bool WebServer::initialize(uint16_t port)
    {
        Serial.println("\n=== Initializing Web Server ===");

        // Initialize OTA Update system
        OTAUpdate::begin();

        // Create HTTP server
        if (server_ != nullptr)
        {
            delete server_;
        }

        server_ = new NetworkServer(port);

        // Ensure port is valid
        if (port == 0)
        {
            Serial.println("ERROR: Invalid port 0");
            return false;
        }

        server_->begin();

        Serial.print("HTTP server started on port ");
        Serial.println(port);

        Serial.println("Web interface ready!");
        Serial.print("Access at: http://");
        NetworkManager &networkManager = NetworkManager::getInstance();
        Serial.print(networkManager.getIPAddress());
        Serial.println("/");

        return true;
    }

    void WebServer::handleClient()
    {
        if (!server_)
        {
            return;
        }

        // Clean up old sessions periodically
        static uint32_t lastCleanup = 0;
        if (millis() - lastCleanup > 60000)
        { // Every minute
            cleanupSessions();
            lastCleanup = millis();
        }

        // Handle HTTP requests
#ifdef USE_ETHERNET
        EthernetClient rawClient = server_->accept(); // Ethernet uses accept()
        if (rawClient)
        {
            Serial.println("New Web Client (Ethernet)");
            NetworkClient client(rawClient);
            handleHttpRequest(client);
        }
#else
        WiFiClient rawClient = server_->accept(); // WiFi also uses accept()
        if (rawClient)
        {
            Serial.println("New Web Client (WiFi)");
            NetworkClient client(rawClient);
            handleHttpRequest(client);
        }
#endif
    }

    void WebServer::handleHttpRequest(NetworkClient &client)
    {
        String request = "";
        String headers = "";
        String body = "";
        bool headersComplete = false;
        int contentLength = 0;
        String method = "";
        String path = "";

        // Wait for data with timeout
        unsigned long timeout = millis() + 1000; // 1 second timeout
        while (!client.available() && millis() < timeout)
        {
            delay(1);
        }

        if (!client.available())
        {
            client.stop();
            return;
        }

        // Read headers line by line
        while (client.connected())
        {
            if (!client.available())
            {
                delay(1);
                continue;
            }

            String line = client.readStringUntil('\n');
            line.trim(); // Remove \r and whitespace

            if (!headersComplete)
            {
                if (line.length() == 0)
                {
                    // Empty line means end of headers
                    headersComplete = true;

                    // Parse request line to determine if this is a firmware upload
                    int firstSpace = request.indexOf(' ');
                    int secondSpace = request.indexOf(' ', firstSpace + 1);

                    if (firstSpace != -1 && secondSpace != -1)
                    {
                        method = request.substring(0, firstSpace);
                        path = request.substring(firstSpace + 1, secondSpace);
                    }

                    // Check if this is a large firmware upload (skip buffering body in String)
                    bool isFirmwareUpload = (path == "/api/upload-firmware" && method == "POST" && contentLength > 32768);

                    if (isFirmwareUpload)
                    {
                        // DON'T read body - let apiUploadFirmware() read it chunk-by-chunk
                        Serial.print("Large firmware upload detected (");
                        Serial.print(contentLength);
                        Serial.println(" bytes) - will stream directly");
                        break; // Exit header reading loop
                    }
                    else if (contentLength > 16384)
                    {
                        // Reject large payloads for non-firmware endpoints
                        Serial.println("Payload too large");
                        sendHttpResponse(client, 413, "text/plain", "Payload Too Large");
                        client.stop();
                        return;
                    }
                    else if (contentLength > 0)
                    {
                        // Normal body reading for small requests (< 32KB)
                        timeout = millis() + 1000;
                        while (body.length() < (size_t)contentLength && millis() < timeout)
                        {
                            if (client.available())
                            {
                                body += (char)client.read();
                            }
                            else
                            {
                                delay(1);
                            }
                        }
                    }
                    break; // Done reading
                }
                else
                {
                    if (request.isEmpty())
                    {
                        request = line;
                    }
                    else
                    {
                        headers += line + "\n";

                        // Check for Content-Length header
                        if (line.startsWith("Content-Length: "))
                        {
                            contentLength = line.substring(16).toInt();
                        }
                    }
                }
            }
        }

        // Validate request line
        if (method.isEmpty() || path.isEmpty())
        {
            Serial.println("Bad request format");
            sendHttpResponse(client, 400, "text/plain", "Bad Request");
            client.stop();
            return;
        }

        Serial.print("HTTP ");
        Serial.print(method);
        Serial.print(" ");
        Serial.println(path);

        // Handle different paths
        if (path == "/" || path == "/index.html")
        {
            sendHtmlPage(client);
        }
        else if (path == "/api/login" && method == "POST")
        {
            handleLogin(client, body);
        }
        else if (path == "/api/upload-firmware" && method == "POST")
        {
            // Special handling for firmware upload (large binary data)
            // For large uploads (>32KB), body will be empty and client stream is still positioned at body start
            apiUploadFirmware(client, headers, contentLength, body);
        }
        else if (path.startsWith("/api/"))
        {
            String authToken = parseAuthToken(headers);
            handleApiRequest(client, path, method, body, authToken);
        }
        else
        {
            sendHttpResponse(client, 404, "text/plain", "Not Found");
        }

        client.stop();
    }

    void WebServer::handleLogin(NetworkClient &client, const String &body)
    {
        // Rate limiting check
        if (loginAttempts_ >= MAX_LOGIN_ATTEMPTS)
        {
            if (millis() - lastLoginAttemptTime_ < LOGIN_LOCKOUT_MS)
            {
                Serial.println("Login blocked by rate limiter");
                sendJsonResponse(client, 429, "{\"error\":\"Too many login attempts. Try again later.\"}");
                return;
            }
            else
            {
                // Reset after lockout period
                loginAttempts_ = 0;
            }
        }

        // Parse JSON body
        DynamicJsonDocument doc(512); // Increase limit slightly but keep reasonable
        DeserializationError error = deserializeJson(doc, body);

        if (error)
        {
            sendJsonResponse(client, 400, "{\"error\":\"Invalid JSON\"}");
            return;
        }

        String username = doc["username"] | "";
        String password = doc["password"] | "";

        // Basic input sanitization
        if (username.length() > 64 || password.length() > 64)
        {
            sendJsonResponse(client, 400, "{\"error\":\"Credentials too long\"}");
            return;
        }

        if (checkCredentials(username, password))
        {
            loginAttempts_ = 0; // Reset on success
            String token = generateSessionToken();

            Session session;
            session.token = token;
            session.lastActivity = millis();
            session.authenticated = true;
            sessions_[token] = session;

            String response = "{\"success\":true,\"token\":\"" + token + "\"}";
            sendJsonResponse(client, 200, response);
            Serial.println("Login successful");
        }
        else
        {
            loginAttempts_++;
            lastLoginAttemptTime_ = millis();
            Serial.print("Login failed. Attempt ");
            Serial.print(loginAttempts_);
            Serial.print("/");
            Serial.println(MAX_LOGIN_ATTEMPTS);

            sendJsonResponse(client, 401, "{\"error\":\"Invalid credentials\"}");
        }
    }

    void WebServer::handleApiRequest(NetworkClient &client, const String &path, const String &method, const String &body, const String &authToken)
    {
        // Check authentication
        Serial.print("API Request: ");
        Serial.print(method);
        Serial.print(" ");
        Serial.println(path);
        Serial.print("Auth token: ");
        Serial.println(authToken.length() > 0 ? authToken.substring(0, 8) + "..." : "(empty)");

        if (!isAuthenticated(authToken))
        {
            Serial.println("Authentication failed!");
            sendJsonResponse(client, 401, "{\"error\":\"Unauthorized\"}");
            return;
        }

        Serial.println("Authentication OK");

        // Update session activity
        if (sessions_.count(authToken))
        {
            sessions_[authToken].lastActivity = millis();
        }

        // Route to appropriate handler
        if (path == "/api/pins" && method == "GET")
        {
            apiGetPins(client);
        }
        else if (path == "/api/pins/set" && method == "POST")
        {
            apiSetPin(client, body);
        }
        else if (path == "/api/status" && method == "GET")
        {
            apiGetStatus(client);
        }
        else if (path == "/api/config" && method == "GET")
        {
            apiGetConfig(client);
        }
        else if (path == "/api/config" && method == "POST")
        {
            apiSetConfig(client, body);
        }
        else if (path == "/api/settings" && method == "GET")
        {
            apiGetSettings(client);
        }
        else if (path == "/api/settings" && method == "POST")
        {
            apiSetSettings(client, body);
        }
        else if (path == "/api/reboot" && method == "POST")
        {
            apiReboot(client);
        }
        else if (path == "/api/logs" && method == "GET")
        {
            apiGetLogs(client);
        }
        else if (path == "/api/logs" && method == "DELETE")
        {
            apiClearLogs(client);
        }
        else if (path == "/api/upload-firmware" && method == "POST")
        {
            // Note: firmware upload is handled separately in handleHttpRequest
            // because it needs special handling for large binary data
            sendJsonResponse(client, 400, "{\"error\":\"Use handleHttpRequest for firmware upload\"}");
        }
        else
        {
            sendJsonResponse(client, 404, "{\"error\":\"Endpoint not found\"}");
        }
    }

    void WebServer::apiGetPins(NetworkClient &client)
    {
        if (!ioController_)
        {
            sendJsonResponse(client, 500, "{\"error\":\"IO controller not initialized\"}");
            return;
        }

        Serial.println("API: Getting pins...");

        DynamicJsonDocument doc(32768); // Increased buffer size for larger pin counts
        JsonArray pinsArray = doc.createNestedArray("pins");

        const auto pinNumbers = ioController_->getAllPinNumbers();
        Serial.print("API: Found ");
        Serial.print(pinNumbers.size());
        Serial.println(" pins");

        for (uint16_t key : pinNumbers)
        {
            IO::IPinHandler *handler = ioController_->getPin(key);
            if (handler)
            {
                const IO::PinConfiguration &config = handler->getConfiguration();
                const IO::PinState &state = handler->getState();

                JsonObject pinObj = pinsArray.createNestedObject();
                pinObj["key"] = key;
                pinObj["pinNumber"] = config.pinNumber;
                pinObj["type"] = static_cast<int>(config.type);
                pinObj["mode"] = static_cast<int>(config.mode);
                pinObj["name"] = config.name;
                pinObj["topic"] = config.mqttTopic;
                pinObj["value"] = state.value;
                pinObj["valid"] = state.isValid;
                pinObj["min"] = config.minValue;
                pinObj["max"] = config.maxValue;
                pinObj["precision"] = config.precision;
                if (!config.unit.empty())
                {
                    pinObj["unit"] = config.unit;
                }
            }
        }

        String response;
        serializeJson(doc, response);
        Serial.print("API: Response size: ");
        Serial.print(response.length());
        Serial.println(" bytes");

        sendJsonResponse(client, 200, response);
        Serial.println("API: Pins response sent");
    }

    void WebServer::apiSetPin(NetworkClient &client, const String &body)
    {
        if (!ioController_)
        {
            sendJsonResponse(client, 500, "{\"error\":\"IO controller not initialized\"}");
            return;
        }

        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, body);

        if (error)
        {
            sendJsonResponse(client, 400, "{\"error\":\"Invalid JSON\"}");
            return;
        }

        String topic = doc["topic"] | "";
        float value = doc["value"] | 0.0f;

        if (topic.isEmpty() || topic.length() > 64)
        {
            sendJsonResponse(client, 400, "{\"error\":\"Invalid topic\"}");
            return;
        }

        IO::IPinHandler *pin = ioController_->getPinByTopic(topic.c_str());
        if (!pin)
        {
            sendJsonResponse(client, 404, "{\"error\":\"Pin not found\"}");
            return;
        }

        if (pin->setValue(value))
        {
            sendJsonResponse(client, 200, "{\"success\":true}");
        }
        else
        {
            sendJsonResponse(client, 500, "{\"error\":\"Failed to set value\"}");
        }
    }

    void WebServer::apiGetStatus(NetworkClient &client)
    {
        Serial.println("API: Getting status...");

        DynamicJsonDocument doc(2048); // Increased for storage info

        doc["uptime"] = millis();

        mbed_stats_heap_t heap_stats;
        mbed_stats_heap_get(&heap_stats);
        doc["freeHeap"] = heap_stats.reserved_size - heap_stats.current_size;

        doc["cpuLoad"] = SystemDiagnostics::getCpuLoad();

        SystemDiagnostics::CpuStats cpuStats = SystemDiagnostics::getCpuBreakdown();
        JsonObject cpuDetail = doc.createNestedObject("cpuDetail");
        cpuDetail["io"] = cpuStats.ioLoad;
        cpuDetail["mqtt"] = cpuStats.mqttLoad;
        cpuDetail["web"] = cpuStats.webLoad;
        cpuDetail["total"] = cpuStats.totalLoad;

        NetworkManager &nm = NetworkManager::getInstance();
        NetworkManager::ConnectionType type = nm.getConnectionType();

        if (type == NetworkManager::ConnectionType::ETHERNET)
        {
            doc["network"] = "ethernet";
            doc["ip"] = nm.getIPAddress().toString();
        }
        else if (type == NetworkManager::ConnectionType::WIFI)
        {
            doc["network"] = "wifi";
            doc["rssi"] = WiFi.RSSI();
            doc["ip"] = nm.getIPAddress().toString();
        }
        else
        {
            doc["network"] = "disconnected";
            doc["ip"] = "0.0.0.0";
        }

        doc["mqttConnected"] = mqttConnected_;

        // Time info
        TimeManager &tm = TimeManager::getInstance();
        doc["time"] = tm.getFormattedTime();
        doc["timeSynced"] = tm.isSynchronized();
        doc["isoTime"] = tm.getIsoTime();

        // Storage info
        SystemDiagnostics::StorageInfo storage = SystemDiagnostics::getStorageUsage();
        if (storage.valid)
        {
            JsonObject storageObj = doc.createNestedObject("storage");
            storageObj["total"] = storage.totalBytes;
            storageObj["used"] = storage.usedBytes;
            storageObj["free"] = storage.freeBytes;
        }

        if (ioController_)
        {
            doc["ioHealth"] = ioController_->isHealthy();
            doc["pinCount"] = ioController_->getAllPinNumbers().size();
        }

        String response;
        serializeJson(doc, response);
        Serial.print("API: Status response size: ");
        Serial.print(response.length());
        Serial.println(" bytes");

        sendJsonResponse(client, 200, response);
        Serial.println("API: Status response sent");
    }

    void WebServer::apiGetConfig(NetworkClient &client)
    {
        Serial.println("API: Getting config...");

        ConfigManager &configMgr = ConfigManager::getInstance();
        String configJson = configMgr.toJson();

        Serial.print("API: Config response size: ");
        Serial.print(configJson.length());
        Serial.println(" bytes");

        sendJsonResponse(client, 200, configJson);
        Serial.println("API: Config response sent");
    }

    void WebServer::apiSetConfig(NetworkClient &client, const String &body)
    {
        ConfigManager &configMgr = ConfigManager::getInstance();

        if (configMgr.loadFromJson(body))
        {
            configMgr.saveToStorage();       // Save to flash
            configMgr.setShouldReload(true); // Trigger hot-reload
            sendJsonResponse(client, 200, "{\"success\":true}");
        }
        else
        {
            sendJsonResponse(client, 400, "{\"error\":\"Invalid configuration\"}");
        }
    }

    void WebServer::apiGetSettings(NetworkClient &client)
    {
        Serial.println("API: Getting settings...");

        NetworkSettings &settings = NetworkSettings::getInstance();
        String settingsJson = settings.toJson();

        Serial.print("API: Settings response size: ");
        Serial.print(settingsJson.length());
        Serial.println(" bytes");

        sendJsonResponse(client, 200, settingsJson);
        Serial.println("API: Settings response sent");
    }

    void WebServer::apiSetSettings(NetworkClient &client, const String &body)
    {
        Serial.println("API: Updating settings...");

        NetworkSettings &settings = NetworkSettings::getInstance();

        if (settings.loadFromJson(body))
        {
            if (settings.saveSettings())
            {
                sendJsonResponse(client, 200, "{\"success\":true,\"message\":\"Settings saved. Restart required.\"}");
                Serial.println("API: Settings saved successfully");
            }
            else
            {
                sendJsonResponse(client, 500, "{\"error\":\"Failed to save settings\"}");
                Serial.println("API: Failed to save settings");
            }
        }
        else
        {
            sendJsonResponse(client, 400, "{\"error\":\"Invalid settings format\"}");
            Serial.println("API: Invalid settings format");
        }
    }

    void WebServer::apiReboot(NetworkClient &client)
    {
        Serial.println("API: Reboot requested...");

        sendJsonResponse(client, 200, "{\"success\":true,\"message\":\"Rebooting device...\"}");
        client.stop();

        // Give time for response to be sent
        delay(1000);

        Serial.println("REBOOTING NOW!");
        Serial.flush();

        // Perform system reset
        NVIC_SystemReset();
    }

    String WebServer::generateSessionToken()
    {
        String token = "";
        const char *charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

        // Enhance entropy with micros() and stack address noise
        uint32_t seed = micros();
        seed ^= (uint32_t)(uintptr_t)&token; // Mix in address of local variable
        randomSeed(seed);

        for (int i = 0; i < 32; i++)
        {
            // Re-seed occasionally to avoid simple PRNG sequences
            if (i % 8 == 0)
            {
                randomSeed(micros() + random(0, 10000));
            }
            token += charset[random(0, 62)];
        }
        return token;
    }

    bool WebServer::isAuthenticated(const String &token)
    {
        if (!sessions_.count(token))
        {
            return false;
        }

        Session &session = sessions_[token];

        // Check session timeout (30 minutes)
        if (millis() - session.lastActivity > 1800000)
        {
            sessions_.erase(token);
            return false;
        }

        return session.authenticated;
    }

    void WebServer::cleanupSessions()
    {
        uint32_t now = millis();
        for (auto it = sessions_.begin(); it != sessions_.end();)
        {
            if (now - it->second.lastActivity > 1800000)
            { // 30 minutes
                it = sessions_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    bool WebServer::checkCredentials(const String &username, const String &password)
    {
        return (username == WEB_USERNAME && password == WEB_PASSWORD);
    }

    String WebServer::parseAuthToken(const String &headers)
    {
        int authIdx = headers.indexOf("Authorization: Bearer ");
        if (authIdx == -1)
        {
            return "";
        }

        int startIdx = authIdx + 22; // Length of "Authorization: Bearer "
        int endIdx = headers.indexOf('\n', startIdx);

        if (endIdx == -1)
        {
            return "";
        }

        String token = headers.substring(startIdx, endIdx);
        token.trim();
        return token;
    }

    void WebServer::sendHttpResponse(NetworkClient &client, int statusCode, const String &contentType, const String &body)
    {
        String statusText;
        switch (statusCode)
        {
        case 200:
            statusText = "OK";
            break;
        case 400:
            statusText = "Bad Request";
            break;
        case 401:
            statusText = "Unauthorized";
            break;
        case 404:
            statusText = "Not Found";
            break;
        case 500:
            statusText = "Internal Server Error";
            break;
        default:
            statusText = "Unknown";
            break;
        }

        // Send headers
        client.println("HTTP/1.1 " + String(statusCode) + " " + statusText);
        client.println("Content-Type: " + contentType);
        client.println("Connection: close");
        client.println("Content-Length: " + String(body.length()));
        client.println();

        // Send body in chunks for large responses
        const size_t chunkSize = 1024;
        size_t sent = 0;
        while (sent < body.length())
        {
            size_t remaining = body.length() - sent;
            size_t toSend = (remaining > chunkSize) ? chunkSize : remaining;
            client.write((const uint8_t *)(body.c_str() + sent), toSend);
            sent += toSend;
            delay(1); // Small delay between chunks
        }

        client.flush(); // Ensure all data is sent
    }

    void WebServer::sendJsonResponse(NetworkClient &client, int statusCode, const String &json)
    {
        sendHttpResponse(client, statusCode, "application/json", json);
    }

    void WebServer::sendHtmlPage(NetworkClient &client)
    {
        // Serve the embedded HTML page
        size_t htmlLen = strlen_P(WEB_HTML);
        char *htmlBuffer = new char[htmlLen + 1];
        strcpy_P(htmlBuffer, WEB_HTML);
        htmlBuffer[htmlLen] = '\0';

        String html(htmlBuffer);
        delete[] htmlBuffer;

        sendHttpResponse(client, 200, "text/html", html);
    }

    void WebServer::apiUploadFirmware(NetworkClient &client, const String &headers, int contentLength, const String &body)
    {
        Serial.println("\n=== OTA Firmware Upload ===");

        // Check authentication
        String authToken = parseAuthToken(headers);
        if (!isAuthenticated(authToken))
        {
            Serial.println("Authentication failed!");
            sendJsonResponse(client, 401, "{\"error\":\"Unauthorized\"}");
            return;
        }

        Serial.println("Authentication OK");

        // Validate firmware size
        if (contentLength == 0)
        {
            sendJsonResponse(client, 400, "{\"error\":\"No firmware data received\"}");
            return;
        }

        if (contentLength > 1048576)
        { // 1MB max (full bank size)
            sendJsonResponse(client, 413, "{\"error\":\"Firmware too large (max 1MB)\"}");
            return;
        }

        Serial.print("Firmware size: ");
        Serial.print(contentLength);
        Serial.println(" bytes");

        // Check if body was already read (small upload < 32KB)
        if (body.length() > 0 && body.length() == (size_t)contentLength)
        {
            Serial.println("Using buffered firmware data (small upload)");

            // Send immediate response
            sendJsonResponse(client, 200,
                             "{\"success\":true,"
                             "\"message\":\"Firmware will be stored to QSPI flash and applied by bootloader on reboot.\","
                             "\"size\":" +
                                 String(body.length()) + ","
                                                         "\"method\":\"Arduino Bootloader OTA\"}");

            delay(500);
            client.stop();

            // Perform OTA update with buffered data
            OTAUpdate::UpdateResult result = OTAUpdate::updateFirmware((const uint8_t *)body.c_str(), body.length());

            if (result != OTAUpdate::UPDATE_SUCCESS)
            {
                Serial.println("\n✗ OTA Update Failed!");
                Serial.print("Error: ");
                Serial.println(OTAUpdate::getErrorMessage(result));
            }
            return;
        }

        // Large upload - use streaming API to avoid RAM buffering
        Serial.println("Using streaming API (large upload)");

        // Begin streaming update
        if (!OTAUpdate::beginStreamingUpdate(contentLength))
        {
            Serial.println("✗ Failed to begin streaming update");
            sendJsonResponse(client, 500, "{\"error\":\"Failed to initialize QSPI storage\"}");
            return;
        }

        // Read firmware data from HTTP body and stream directly to QSPI
        const uint32_t chunkSize = 4096; // 4KB chunks
        uint8_t *chunk = (uint8_t *)malloc(chunkSize);
        if (!chunk)
        {
            Serial.println("✗ Failed to allocate chunk buffer");
            OTAUpdate::abortStreamingUpdate();
            sendJsonResponse(client, 500, "{\"error\":\"Out of memory\"}");
            return;
        }

        uint32_t bytesRead = 0;
        unsigned long timeout = millis() + 60000; // 60 second timeout for large files
        uint32_t lastProgress = 0;
        bool success = true;

        while (bytesRead < (uint32_t)contentLength && millis() < timeout)
        {
            // Read chunk from network
            uint32_t chunkBytesRead = 0;
            uint32_t bytesToRead = min(chunkSize, (uint32_t)contentLength - bytesRead);

            while (chunkBytesRead < bytesToRead && millis() < timeout)
            {
                if (client.available())
                {
                    chunk[chunkBytesRead++] = client.read();
                }
                else
                {
                    delay(1);
                }
            }

            if (chunkBytesRead == 0)
            {
                Serial.println("✗ Timeout reading from network");
                success = false;
                break;
            }

            // Write chunk to QSPI
            if (!OTAUpdate::writeStreamChunk(chunk, chunkBytesRead))
            {
                Serial.println("✗ Failed to write chunk to QSPI");
                success = false;
                break;
            }

            bytesRead += chunkBytesRead;

            // Print progress every 10%
            uint32_t progress = (bytesRead * 100) / contentLength;
            if (progress >= lastProgress + 10)
            {
                Serial.print("Reading: ");
                Serial.print(progress);
                Serial.println("%");
                lastProgress = progress;
            }
        }

        free(chunk);

        if (!success || bytesRead < (uint32_t)contentLength)
        {
            Serial.print("✗ Failed: read ");
            Serial.print(bytesRead);
            Serial.print(" of ");
            Serial.print(contentLength);
            Serial.println(" bytes");
            OTAUpdate::abortStreamingUpdate();
            sendJsonResponse(client, 500, "{\"error\":\"Upload failed\"}");
            return;
        }

        Serial.print("✓ Read ");
        Serial.print(bytesRead);
        Serial.println(" bytes from HTTP stream");

        // Send immediate response to client
        sendJsonResponse(client, 200,
                         "{\"success\":true,"
                         "\"message\":\"Firmware will be stored to QSPI flash and applied by bootloader on reboot.\","
                         "\"size\":" +
                             String(bytesRead) + ","
                                                 "\"method\":\"Arduino Bootloader OTA (Streaming)\"}");

        delay(500);
        client.stop();

        // Finalize streaming update (this will configure bootloader and reboot)
        OTAUpdate::UpdateResult result = OTAUpdate::finalizeStreamingUpdate();

        // If we reach here, something went wrong
        if (result != OTAUpdate::UPDATE_SUCCESS)
        {
            Serial.println("\n✗ OTA Update Failed!");
            Serial.print("Error: ");
            Serial.println(OTAUpdate::getErrorMessage(result));
        }
    }

    void WebServer::apiGetLogs(NetworkClient &client)
    {
        Serial.println("API: Getting logs...");

        Logging::Logger &logger = Logging::Logger::getInstance();

        // Increased line count from 100 to 500 for more context
        // But capped by memory limits in Logger::readLogs
        String logs = logger.readLogs(500);

        DynamicJsonDocument doc(32768); // Double the buffer size to 32KB
        doc["logs"] = logs;
        doc["size"] = logger.getLogFileSize();
        doc["count"] = logger.getLogCount();

        String response;
        // Check if serialization succeeds
        if (serializeJson(doc, response) == 0)
        {
            Serial.println("API: Error serializing logs (buffer too small?)");
            // Fallback to error message or partial logs
            sendJsonResponse(client, 500, "{\"error\":\"Log too large to buffer\"}");
            return;
        }

        sendJsonResponse(client, 200, response);
        Serial.println("API: Logs response sent");
    }

    void WebServer::apiClearLogs(NetworkClient &client)
    {
        Serial.println("API: Clearing logs...");

        Logging::Logger &logger = Logging::Logger::getInstance();
        logger.clearLogs();

        sendJsonResponse(client, 200, "{\"success\":true,\"message\":\"Logs cleared\"}");
        Serial.println("API: Logs cleared");
    }

} // namespace Web
