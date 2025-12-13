#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <Arduino_ConnectionHandler.h>
#include <NTPClient.h>
#include <Udp.h>

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
     * @brief Provide the Arduino ConnectionHandler used by the application.
     * TimeManager never manages the network itself; it only reads time via NTP when connected.
     */
    void setConnectionHandler(ConnectionHandler *handler);

    /**
     * @brief Initialize NTP client
     * Call this after the ConnectionHandler is configured (and typically after it connected).
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

    ConnectionHandler *connectionHandler_;
    NetworkAdapter lastInterface_;
    UDP *udp_; // Owned by ConnectionHandler; never delete.
    NTPClient *ntpClient_;
    bool isSynchronized_;
    unsigned long lastSyncTime_;
    unsigned long initTime_;
    String currentNtpServer_;
};

#endif // TIME_MANAGER_H
