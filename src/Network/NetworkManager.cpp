#include "NetworkManager.h"
#include "NetworkSettings.h"
#include "credentials.h"
#include "Core/Logger.h"

// Initialize static instance
NetworkManager *NetworkManager::instance = nullptr;

NetworkManager &NetworkManager::getInstance()
{
    static NetworkManager instance;
    return instance;
}

NetworkManager::NetworkManager()
    : lastCheck_(0), status_(ConnectionStatus::DISCONNECTED), connectionType_(ConnectionType::NONE), ethernetConnected_(false), wifiConnected_(false)
{
    instance = this;
}

bool NetworkManager::connect()
{
    NetworkSettings &settings = NetworkSettings::getInstance();
    // Ensure settings are loaded
    if (!settings.isConfigured())
    {
        settings.loadSettings();
    }
    const auto &wifiConfig = settings.getWiFiConfig();

    ssid_ = wifiConfig.ssid.length() > 0 ? wifiConfig.ssid : String(WIFI_SSID);
    password_ = wifiConfig.password.length() > 0 ? wifiConfig.password : String(WIFI_PASSWORD);

    Serial.println("\n=== Initializing Network Manager ===");
    LOG_INFO("Initializing Network Manager");

    // Try Ethernet first
    if (connectEthernet())
    {
        return true;
    }

    // Fallback to WiFi
    if (connectWiFi())
    {
        return true;
    }

    status_ = ConnectionStatus::ERROR;
    LOG_ERROR("Failed to initialize network");
    return false;
}

bool NetworkManager::connectEthernet()
{
    Serial.println("Checking Ethernet...");
    
    // Note: PortentaEthernet usually requires no begin() arguments for DHCP, 
    // or begin(ip, dns, gateway, subnet).
    // Using automatic configuration (DHCP)
    
    // For Portenta, linkStatus() is reliable
    if (Ethernet.linkStatus() == LinkON)
    {
        Serial.println("Ethernet cable detected.");
        // Portenta Ethernet uses built-in MAC address by default if begin() is called without arguments
        // or passed just the MAC address. 
        // The Portenta_Ethernet library's begin() with no args starts DHCP with hardware MAC.
        if (Ethernet.begin() == 0) {
            Serial.println("Ethernet DHCP failed");
            return false;
        }
        
        // Give it a moment to get IP
        delay(1000);
        
        if (Ethernet.localIP() != IPAddress(0,0,0,0)) {
             Serial.print("Ethernet Connected. IP: ");
             Serial.println(Ethernet.localIP());
             
             connectionType_ = ConnectionType::ETHERNET;
             status_ = ConnectionStatus::CONNECTED;
             ethernetConnected_ = true;
             LOG_INFO("Ethernet connected. IP: " + Ethernet.localIP().toString());
             return true;
        }
    }
    
    Serial.println("Ethernet not available.");
    return false;
}

bool NetworkManager::connectWiFi()
{
    if (ssid_.length() == 0)
    {
        Serial.println("No WiFi credentials.");
        return false;
    }

    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid_);

    WiFi.begin(ssid_.c_str(), password_.c_str());
    
    // We don't block indefinitely here, but we can wait a bit for initial connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) // Increased to 20 seconds
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("WiFi Connected. IP: ");
        Serial.println(WiFi.localIP());
        
        connectionType_ = ConnectionType::WIFI;
        status_ = ConnectionStatus::CONNECTED;
        wifiConnected_ = true;
        LOG_INFO("WiFi connected. IP: " + WiFi.localIP().toString());
        return true;
    }

    Serial.print("WiFi connection failed. Status code: ");
    Serial.println(WiFi.status());
    return false;
}

bool NetworkManager::isConnected()
{
    if (connectionType_ == ConnectionType::ETHERNET)
    {
        return Ethernet.linkStatus() == LinkON && Ethernet.localIP() != IPAddress(0,0,0,0);
    }
    else if (connectionType_ == ConnectionType::WIFI)
    {
        return WiFi.status() == WL_CONNECTED;
    }
    return false;
}

IPAddress NetworkManager::getIPAddress()
{
    if (connectionType_ == ConnectionType::ETHERNET)
    {
        return Ethernet.localIP();
    }
    return WiFi.localIP();
}

void NetworkManager::maintain()
{
    unsigned long now = millis();
    if (now - lastCheck_ < 10000) // Check every 10 seconds (relaxed from 5)
    {
        return;
    }
    lastCheck_ = now;

    // If never connected or disconnected, try to connect
    if (status_ == ConnectionStatus::DISCONNECTED || status_ == ConnectionStatus::ERROR) {
        Serial.println("NetworkManager: Attempting to reconnect...");
        connect(); 
        return;
    }

    checkConnection();
}

void NetworkManager::checkConnection()
{
    bool wasConnected = (status_ == ConnectionStatus::CONNECTED);
    bool currentlyConnected = isConnected();

    if (wasConnected && !currentlyConnected)
    {
        Serial.println("Network lost! Attempting reconnect...");
        status_ = ConnectionStatus::DISCONNECTED;
        LOG_WARNING("Network connection lost");
        
        // Attempt reconnect based on type
        if (connectionType_ == ConnectionType::ETHERNET)
        {
             // Check if still plugged in
             if (Ethernet.linkStatus() == LinkON) {
                 Ethernet.begin(); // Renew DHCP
             } else {
                 // Fallback to WiFi if Ethernet unplugged?
                 // For now, just try to reconnect same interface
             }
        }
        else if (connectionType_ == ConnectionType::WIFI)
        {
             WiFi.disconnect();
             WiFi.begin(ssid_.c_str(), password_.c_str());
        }
    }
    else if (!wasConnected && currentlyConnected)
    {
        Serial.println("Network re-established.");
        status_ = ConnectionStatus::CONNECTED;
        LOG_INFO("Network connection re-established. IP: " + getIPAddress().toString());
    }
    else if (!wasConnected && !currentlyConnected)
    {
         // Still disconnected, retry logic could go here
         // For WiFi, WiFi.begin() automatically reconnects usually, but calling it again helps
         if (connectionType_ == ConnectionType::WIFI && WiFi.status() != WL_CONNECTED) {
             WiFi.begin(ssid_.c_str(), password_.c_str());
         }
         else if (connectionType_ == ConnectionType::ETHERNET && Ethernet.linkStatus() == LinkON) {
              Ethernet.begin();
         }
    }
}

Client &NetworkManager::getClient()
{
    if (connectionType_ == ConnectionType::ETHERNET)
    {
        return ethernetClient_;
    }
    return wifiClient_;
}
