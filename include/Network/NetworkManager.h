#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PortentaEthernet.h>

/**
 * @brief Manages network connectivity using native WiFi and Ethernet libraries.
 *
 * Prioritizes Ethernet over WiFi.
 * Handles connection lifecycle and provides a unified client.
 */
class NetworkManager
{
public:
    enum class ConnectionStatus
    {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        ERROR
    };

    enum class ConnectionType
    {
        NONE,
        ETHERNET,
        WIFI
    };

    /**
     * @brief Get singleton instance
     */
    static NetworkManager &getInstance();

    /**
     * @brief Initialize and connect to network
     * @return true if initialization started successfully
     */
    bool connect();

    /**
     * @brief Check if network is connected
     * @return true if connected
     */
    bool isConnected();

    /**
     * @brief Get current IP address
     * @return IP address or 0.0.0.0 if not connected
     */
    IPAddress getIPAddress();

    /**
     * @brief Maintain network connection (call in loop)
     */
    void maintain();

    /**
     * @brief Get current connection status
     */
    ConnectionStatus getStatus() const { return status_; }

    /**
     * @brief Get current connection type
     */
    ConnectionType getConnectionType() const { return connectionType_; }

    /**
     * @brief Get network client for MQTT/HTTP
     */
    Client &getClient();

    static NetworkManager *instance;

private:
    NetworkManager();
    NetworkManager(const NetworkManager &) = delete;
    NetworkManager &operator=(const NetworkManager &) = delete;

    // Helpers
    bool connectEthernet();
    bool connectWiFi();
    void checkConnection();

    String ssid_;
    String password_;
    
    unsigned long lastCheck_;
    ConnectionStatus status_;
    ConnectionType connectionType_;

    // Clients
    WiFiClient wifiClient_;
    EthernetClient ethernetClient_;
    
    // Wrappers for simplified state management
    bool ethernetConnected_;
    bool wifiConnected_;
};

#endif // NETWORK_MANAGER_H
