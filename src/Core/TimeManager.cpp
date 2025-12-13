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
    : connectionHandler_(nullptr),
      lastInterface_(NetworkAdapter::NONE),
      udp_(nullptr),
      ntpClient_(nullptr),
      isSynchronized_(false),
      lastSyncTime_(0),
      initTime_(0)
{
}

void TimeManager::setConnectionHandler(ConnectionHandler *handler)
{
    connectionHandler_ = handler;
}

void TimeManager::begin()
{
    if (!connectionHandler_)
    {
        LOG_WARNING("Cannot start TimeManager: ConnectionHandler not set");
        return;
    }

    // Ensure the connection state machine runs and is connected
    if (connectionHandler_->check() != NetworkConnectionState::CONNECTED)
    {
        LOG_WARNING("Cannot start TimeManager: Network not connected");
        return;
    }

    const NetworkAdapter currentInterface = connectionHandler_->getInterface();

    // If we are already initialized and interface hasn't changed, do nothing
    if (ntpClient_ != nullptr && currentInterface == lastInterface_)
    {
        return;
    }

    // Clean up existing instances if connection changed
    if (ntpClient_)
    {
        delete ntpClient_;
        ntpClient_ = nullptr;
    }

    // Use UDP provided by the ConnectionHandler (owned by it).
    // Some adapters (e.g., LoRa/Notecard) do not expose UDP/Client.
#if defined(BOARD_HAS_LORA) || defined(BOARD_HAS_NOTECARD)
    LOG_WARNING("TimeManager: UDP not available on this adapter");
    udp_ = nullptr;
    return;
#else
    udp_ = &connectionHandler_->getUDP();
#endif

    // Initialize NTP Client
    const auto &ntpConfig = NetworkSettings::getInstance().getNTPConfig();
    currentNtpServer_ = ntpConfig.server;

    // Update interval: 60 seconds for initial sync, can be increased later
    ntpClient_ = new NTPClient(*udp_, currentNtpServer_.c_str(), ntpConfig.timeOffset, 60000);
    ntpClient_->begin();

    lastInterface_ = currentInterface;
    initTime_ = millis();

    const char *ifaceStr = "unknown";
    if (currentInterface == NetworkAdapter::ETHERNET)
        ifaceStr = "ethernet";
    else if (currentInterface == NetworkAdapter::WIFI)
        ifaceStr = "wifi";

    LOG_INFO("TimeManager initialized using " + String(ifaceStr));

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

    if (!connectionHandler_)
        return;

    // Keep connection workflow running; only sync when connected
    if (connectionHandler_->check() != NetworkConnectionState::CONNECTED)
        return;

    // Wait for network stack to stabilize
    if (millis() - initTime_ < 3000)
        return;

    // Check if interface changed
    if (connectionHandler_->getInterface() != lastInterface_)
    {
        LOG_INFO("Network interface changed, re-initializing TimeManager");
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
