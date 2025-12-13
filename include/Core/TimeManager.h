#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EthernetUdp.h>
#include "Network/NetworkManager.h"

/**
 * @brief Manages RTC synchronization using NTP
 *
 * Uses NTPClient to fetch time from pool.ntp.org and updates
 * the Portenta H7 internal RTC.
 */
class TimeManager
{
public:
    /**
     * @brief Get singleton instance
     */
    static TimeManager &getInstance();

    /**
     * @brief Initialize NTP client
     * Call this after NetworkManager is connected
     */
    void begin();

    /**
     * @brief Update time synchronization
     * Call this in the main loop
     */
    void update();

    /**
     * @brief Check if time has been synchronized at least once
     */
    bool isSynchronized() const { return isSynchronized_; }

    /**
     * @brief Get current time as formatted string
     * @return String "YYYY-MM-DD HH:MM:SS"
     */
    String getFormattedTime();

    /**
     * @brief Get current time as ISO8601 string
     * @return String "YYYY-MM-DDTHH:MM:SSZ"
     */
    String getIsoTime();

private:
    TimeManager();
    TimeManager(const TimeManager &) = delete;
    TimeManager &operator=(const TimeManager &) = delete;

    // NOTE: arduino::UDP has a non-virtual destructor, so never delete via UDP*.
    // We keep concrete UDP implementations as members and point udp_ at the active one.
    WiFiUDP wifiUdp_;
    EthernetUDP ethernetUdp_;
    UDP *udp_;
    NTPClient *ntpClient_;
    bool isSynchronized_;
    unsigned long lastSyncTime_;
    unsigned long initTime_;
    NetworkManager::ConnectionType lastConnectionType_;
    String currentNtpServer_;
};

#endif // TIME_MANAGER_H
