#ifndef SYSTEM_DIAGNOSTICS_H
#define SYSTEM_DIAGNOSTICS_H

#include <Arduino.h>

/**
 * @brief System diagnostics and OTA validation
 *
 * Single Responsibility: Boot-time diagnostic checks
 * - Bootloader version checking
 * - OTA attempt detection (RTC registers)
 * - UPDATE.BIN validation
 * - Firmware format verification
 */
class SystemDiagnostics
{
public:
    struct StorageInfo
    {
        uint64_t totalBytes;
        uint64_t usedBytes;
        uint64_t freeBytes;
        bool valid;
    };

    /**
     * @brief Get usage statistics for the user data partition (QSPI Partition 3)
     */
    static StorageInfo getStorageUsage();

    /**
     * @brief Get current CPU load percentage (0-100)
     * Calculates usage based on idle time delta since last call.
     * @return float CPU load percentage
     */
    static float getCpuLoad();

    enum class TaskType
    {
        IO,
        MQTT,
        WEB,
        OTHER
    };

    struct CpuStats
    {
        float ioLoad;
        float mqttLoad;
        float webLoad;
        float totalLoad; // From mbed stats
    };

    /**
     * @brief Record execution time for a specific task
     */
    static void recordTask(TaskType type, uint32_t microsDuration);

    /**
     * @brief Get detailed CPU breakdown
     */
    static CpuStats getCpuBreakdown();

    /**
     * @brief Run complete OTA diagnostic check at boot
     *
     * Checks:
     * - Bootloader version compatibility
     * - RTC backup registers for OTA magic
     * - UPDATE.BIN presence and validity
     * - Firmware format validation
     */
    static void runOtaDiagnostics();

private:
    SystemDiagnostics() = default;

    /**
     * @brief Get bootloader version from flash
     * @return Bootloader version number
     */
    static uint32_t getBootloaderVersion();

    /**
     * @brief Check RTC backup registers for OTA magic
     * @return true if OTA was attempted but not completed
     */
    static bool checkRtcOtaMagic();

    /**
     * @brief Check if UPDATE.BIN exists in QSPI and validate format
     */
    static void checkUpdateBin();

    /**
     * @brief Validate firmware format (ARM reset vector)
     * @param file Firmware file handle
     * @return true if valid .bin format
     */
    static bool validateFirmwareFormat(FILE *file);
};

#endif // SYSTEM_DIAGNOSTICS_H
