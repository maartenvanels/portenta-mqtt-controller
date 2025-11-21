#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>
#include "stm32h7xx_hal.h"
#include <Arduino_UnifiedStorage.h>
#include "lzss.h"

// Arduino Portenta Bootloader OTA Implementation
// Uses the same method as Arduino_Portenta_OTA library:
// 1. Store firmware to QSPI flash
// 2. Write magic value 0x07AA to RTC backup registers
// 3. Reboot -> bootloader flashes firmware from QSPI

// RAM address range for validation
#ifndef RAM_BASE
#define RAM_BASE            0x20000000
#endif
#ifndef RAM_END
#define RAM_END             0x24080000
#endif

// Maximum firmware size (768KB - same as PlatformIO config)
#define MAX_FIRMWARE_SIZE   0x000C0000

// OTA filename on QSPI flash (same as Arduino_Portenta_OTA)
#define OTA_FIRMWARE_FILE   "UPDATE.BIN"

// Arduino Portenta OTA Storage Type Flags (from Arduino_Portenta_OTA)
#define APOTA_QSPI_FLASH_FLAG   (1 << 2)  // 4
#define APOTA_SDCARD_FLAG       (1 << 3)  // 8
#define APOTA_FATFS_FLAG        (1 << 5)  // 32
#define APOTA_MBR_FLAG          (1 << 7)  // 128

// Storage type: QSPI Flash with FATFS and MBR partitioning (CRITICAL!)
// MUST use MBR flag because QSPIFormat creates MBR partition table
#define QSPI_FLASH_FATFS_MBR    (APOTA_QSPI_FLASH_FLAG | APOTA_FATFS_FLAG | APOTA_MBR_FLAG)  // 164

// Arduino Portenta Bootloader Magic Value
#define BOOTLOADER_MAGIC        0x07AA

class OTAUpdate {
public:
    enum UpdateResult {
        UPDATE_SUCCESS = 0,
        UPDATE_ERROR_SIZE = -1,
        UPDATE_ERROR_FORMAT = -2,
        UPDATE_ERROR_STORAGE_INIT = -3,
        UPDATE_ERROR_STORAGE_WRITE = -4,
        UPDATE_ERROR_RTC = -5,
        UPDATE_ERROR_BOOTLOADER_VERSION = -6,
        UPDATE_ERROR_NO_PARTITIONS = -7,
        UPDATE_ERROR_MOUNT_FAILED = -8,
        UPDATE_ERROR_FILE_CREATE = -9,
        UPDATE_ERROR_DECOMPRESS = -10,
        UPDATE_ERROR_SIZE_MISMATCH = -11
    };

    // Watchdog feed callback type (same as Arduino_Portenta_OTA)
    typedef void (*WatchdogFeedCallback)(void);

    // Progress callback type (percent complete, status message)
    typedef void (*ProgressCallback)(uint8_t percent, const char* statusMessage);

    // Initialize OTA system
    static void begin();

    // Check if bootloader supports OTA (version >= 22)
    static bool isOtaCapable();

    // Main OTA update function (Arduino bootloader method)
    static UpdateResult updateFirmware(const uint8_t* firmwareData, uint32_t firmwareSize);

    // Streaming API for large firmware updates (to avoid RAM buffering)
    static bool beginStreamingUpdate(uint32_t expectedSize);
    static bool writeStreamChunk(const uint8_t* data, uint32_t size);
    static UpdateResult finalizeStreamingUpdate();
    static void abortStreamingUpdate();

    // Validate firmware format
    static bool validateFirmwareFormat(const uint8_t* firmwareData, uint32_t firmwareSize);

    // Get human-readable error message
    static const char* getErrorMessage(UpdateResult result);

    // Set watchdog feed callback (called every 32KB to prevent watchdog timeout)
    static void setWatchdogCallback(WatchdogFeedCallback callback);

    // Set progress callback (called with percent complete and status)
    static void setProgressCallback(ProgressCallback callback);

    // Feed watchdog manually if callback is set
    static void feedWatchdog();

private:
    // Write firmware to QSPI flash
    static bool writeFirmwareToQSPI(const uint8_t* data, uint32_t size);

    // Write bootloader parameters to RTC backup registers
    static void writeBootloaderParams(uint32_t storageType, uint32_t dataOffset, uint32_t programLength);

    // Trigger system reset for bootloader to apply update
    static void triggerBootloaderUpdate();

    // Callbacks
    static WatchdogFeedCallback watchdogCallback_;
    static ProgressCallback progressCallback_;

    // Streaming state (for beginStreamingUpdate/writeStreamChunk/finalizeStreamingUpdate)
    static FILE* streamFile_;
    static uint32_t streamBytesWritten_;
    static uint32_t streamExpectedSize_;
    static void* streamFs_;
    static void* streamBdMbr_;
    static void* streamBdRaw_;

    // LZSS decompression state
    static LZSSDecoder* lzssDecoder_;
    static uint32_t streamInputBytesProcessed_;  // Total input bytes (compressed .ota)
    static uint32_t streamOutputBytesWritten_;   // Total output bytes (decompressed raw bin)
    static const uint32_t OTA_HEADER_SIZE = 20;  // 4B len + 4B CRC + 4B magic + 8B version

    // Format detection
    static bool isOtaFile_;
    static bool formatChecked_;
};

#endif // OTA_UPDATE_H
