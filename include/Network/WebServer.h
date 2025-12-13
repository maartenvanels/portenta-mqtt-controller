#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <map>

// Network includes based on configuration
#ifdef USE_ETHERNET
    #include <PortentaEthernet.h>
    #include <Ethernet.h>
#else
    #include <WiFi.h>
#endif

#include "NetworkClient.h"
#include "IO/IoController.h"
#include "Core/ConfigManager.h"

class ConnectionHandler;

namespace Web {

// Session management
struct Session {
    String token;
    uint32_t lastActivity;
    bool authenticated;
};

class WebServer {
public:
    static WebServer& getInstance();

    bool initialize(uint16_t port = 80);
    void handleClient();

    void setIoController(IO::IoController* controller) { ioController_ = controller; }
    void setMqttConnected(bool connected) { mqttConnected_ = connected; }
    void setConnectionHandler(ConnectionHandler* handler) { connectionHandler_ = handler; }

private:
    WebServer() : server_(nullptr), loginAttempts_(0), lastLoginAttemptTime_(0), mqttConnected_(false), ioController_(nullptr), connectionHandler_(nullptr) {}
    ~WebServer() { if (server_) delete server_; }

    // Prevent copying
    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;

    // HTTP server - type depends on network configuration
#ifdef USE_ETHERNET
    EthernetServer* server_;
#else
    WiFiServer* server_;
#endif

    // Session management
    std::map<String, Session> sessions_;
    String generateSessionToken();
    bool isAuthenticated(const String& token);
    void cleanupSessions();

    // HTTP handlers - unified interface using NetworkClient wrapper
    void handleHttpRequest(NetworkClient& client);
    void handleLogin(NetworkClient& client, const String& body);
    void handleApiRequest(NetworkClient& client, const String& path, const String& method, const String& body, const String& authToken);

    // API endpoints
    void apiGetPins(NetworkClient& client);
    void apiSetPin(NetworkClient& client, const String& body);
    void apiGetStatus(NetworkClient& client);
    void apiGetConfig(NetworkClient& client);
    void apiSetConfig(NetworkClient& client, const String& body);
    void apiGetSettings(NetworkClient& client);
    void apiSetSettings(NetworkClient& client, const String& body);
    void apiReboot(NetworkClient& client);
    void apiUploadFirmware(NetworkClient& client, const String& headers, int contentLength, const String& body);
    void apiGetLogs(NetworkClient& client);
    void apiClearLogs(NetworkClient& client);

    // Response helpers
    void sendHttpResponse(NetworkClient& client, int statusCode, const String& contentType, const String& body);
    void sendJsonResponse(NetworkClient& client, int statusCode, const String& json);
    void sendHtmlPage(NetworkClient& client);

    // Authentication
    bool checkCredentials(const String& username, const String& password);
    String parseAuthToken(const String& headers);

    // Rate limiting
    uint32_t loginAttempts_ = 0;
    uint32_t lastLoginAttemptTime_ = 0;
    static const uint32_t MAX_LOGIN_ATTEMPTS = 5;
    static const uint32_t LOGIN_LOCKOUT_MS = 60000; // 1 minute

    bool mqttConnected_;
    IO::IoController* ioController_;
    ConnectionHandler* connectionHandler_;
};

} // namespace Web

#endif // WEBSERVER_H
