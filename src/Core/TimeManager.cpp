#include "Core/TimeManager.h"
#include "Network/NetworkSettings.h"
#include "Core/Logger.h"
#include <time.h>

// Ensure set_time is available
extern "C" void set_time(time_t t);

TimeManager &TimeManager::getInstance()
{
    static TimeManager instance;
    return instance;
}

TimeManager::TimeManager()
    : udp_(nullptr),
      ntpClient_(nullptr),
      isSynchronized_(false),
      lastSyncTime_(0),
      initTime_(0),
      lastConnectionType_(NetworkManager::ConnectionType::NONE)
{
}

void TimeManager::begin()
{
    NetworkManager &netManager = NetworkManager::getInstance();

    if (!netManager.isConnected())
    {
        LOG_WARNING("Cannot start TimeManager: Network not connected");
        return;
    }

    NetworkManager::ConnectionType currentType = netManager.getConnectionType();

    // If we are already initialized and connection type hasn't changed, do nothing
    if (udp_ != nullptr && currentType == lastConnectionType_)
    {
        return;
    }

    // Clean up existing instances if connection changed
    if (ntpClient_)
    {
        delete ntpClient_;
        ntpClient_ = nullptr;
    }

    // Create appropriate UDP instance
    if (currentType == NetworkManager::ConnectionType::ETHERNET)
    {
        Serial.println("TimeManager: Initializing for Ethernet");
        udp_ = &ethernetUdp_;
    }
    else if (currentType == NetworkManager::ConnectionType::WIFI)
    {
        Serial.println("TimeManager: Initializing for WiFi");
        udp_ = &wifiUdp_;
    }
    else
    {
        LOG_ERROR("TimeManager: Unknown connection type");
        udp_ = nullptr;
        return;
    }

    // Initialize NTP Client
    const auto &ntpConfig = NetworkSettings::getInstance().getNTPConfig();
    currentNtpServer_ = ntpConfig.server;

    // Update interval: 60 seconds for initial sync, can be increased later
    ntpClient_ = new NTPClient(*udp_, currentNtpServer_.c_str(), ntpConfig.timeOffset, 60000);
    ntpClient_->begin();

    lastConnectionType_ = currentType;
    initTime_ = millis();
    LOG_INFO("TimeManager initialized using " + String(currentType == NetworkManager::ConnectionType::ETHERNET ? "Ethernet" : "WiFi"));

    // Check if RTC is already valid (e.g. battery backup)
    time_t now = time(NULL);
    if (now > 1600000000)
    { // Approx year 2020
        isSynchronized_ = true;
        Serial.print("RTC already valid: ");
        Serial.println(getFormattedTime());
        LOG_INFO("RTC preserved (battery backup?): " + getFormattedTime());
    }
}

void TimeManager::update()
{
    if (!ntpClient_)
        return;

    // Check if network is still connected
    NetworkManager &netManager = NetworkManager::getInstance();
    if (!netManager.isConnected())
        return;

    // Wait for network stack to stabilize
    if (millis() - initTime_ < 3000)
        return;

    // Check if connection type changed
    if (netManager.getConnectionType() != lastConnectionType_)
    {
        LOG_INFO("Network connection type changed, re-initializing TimeManager");
        begin();
        return;
    }

    // Update NTP client
    // update() returns true if time was updated
    if (ntpClient_->update())
    {
        unsigned long epochTime = ntpClient_->getEpochTime();

        // Set internal RTC
        set_time(epochTime);

        if (!isSynchronized_)
        {
            isSynchronized_ = true;
            Serial.print("Time Synchronized: ");
            Serial.println(getFormattedTime());
            LOG_INFO("Time synchronized: " + getFormattedTime());

            // After first sync, we can reduce update frequency to configured interval
            const auto &ntpConfig = NetworkSettings::getInstance().getNTPConfig();
            ntpClient_->setUpdateInterval(ntpConfig.updateInterval);
        }
        else
        {
            // Log periodic sync (verbose only)
            // LOG_DEBUG("Time re-synced: " + getFormattedTime());
        }
    }
}

String TimeManager::getFormattedTime()
{
    // Use internal RTC
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);

    char buffer[32];
    // Format: "YYYY-MM-DD HH:MM:SS"
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    return String(buffer);
}

String TimeManager::getIsoTime()
{
    // Use internal RTC
    time_t now = time(NULL);
    struct tm *timeinfo = gmtime(&now); // ISO usually implies UTC/Z

    char buffer[32];
    // Format: "YYYY-MM-DDTHH:MM:SSZ"
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", timeinfo);

    return String(buffer);
}
