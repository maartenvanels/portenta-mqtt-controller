#include "SystemDiagnostics.h"
#include <BlockDevice.h>
#include <MBRBlockDevice.h>
#include <FATFileSystem.h>
#include <LittleFileSystem.h>
#include <sys/stat.h>
#include <mbed_stats.h>

// RTC handle for backup register access
extern RTC_HandleTypeDef RTCHandle;

// Bootloader address in flash
// Bootloader address in flash
#ifdef UNIT_TEST
static uint8_t mock_bootloader_memory[0x20000]; // 128KB buffer to cover 0x1F000 offset
#define BOOTLOADER_ADDR ((uintptr_t)mock_bootloader_memory)
#else
#define BOOTLOADER_ADDR 0x8000000
#endif

// CPU Load breakdown accumulators
static uint32_t accIO = 0;
static uint32_t accMQTT = 0;
static uint32_t accWeb = 0;
static uint32_t lastBreakdownTime = 0;
static SystemDiagnostics::CpuStats cachedStats = {0};

void SystemDiagnostics::recordTask(TaskType type, uint32_t microsDuration)
{
    switch (type)
    {
    case TaskType::IO:
        accIO += microsDuration;
        break;
    case TaskType::MQTT:
        accMQTT += microsDuration;
        break;
    case TaskType::WEB:
        accWeb += microsDuration;
        break;
    default:
        break;
    }
}

SystemDiagnostics::CpuStats SystemDiagnostics::getCpuBreakdown()
{
    uint32_t now = millis();
    if (now - lastBreakdownTime >= 1000)
    {
        // Update stats every second
        float periodMicros = (now - lastBreakdownTime) * 1000.0f;
        if (periodMicros > 0)
        {
            cachedStats.ioLoad = (accIO / periodMicros) * 100.0f;
            cachedStats.mqttLoad = (accMQTT / periodMicros) * 100.0f;
            cachedStats.webLoad = (accWeb / periodMicros) * 100.0f;
        }

        // Reset accumulators
        accIO = 0;
        accMQTT = 0;
        accWeb = 0;
        lastBreakdownTime = now;
    }

    cachedStats.totalLoad = getCpuLoad();
    return cachedStats;
}

SystemDiagnostics::StorageInfo SystemDiagnostics::getStorageUsage()
{
    StorageInfo info = {0, 0, 0, false};

    auto bd_raw = mbed::BlockDevice::get_default_instance();
    if (!bd_raw || bd_raw->init() != 0)
    {
        return info;
    }

    // Partition 3 is User Data (LittleFS)
    auto bd_mbr = new mbed::MBRBlockDevice(bd_raw, 3);
    auto fs = new mbed::LittleFileSystem("user");

    if (fs->mount(bd_mbr) == 0)
    {
        // Get total size from block device
        info.totalBytes = bd_mbr->size();

        // Note: dirent.h is not available in this toolchain, so we cannot iterate files
        // to calculate used space. Reporting total size only.
        info.usedBytes = 0;
        info.freeBytes = info.totalBytes; // Optimistic assumption since we can't calculate used
        info.valid = true;

        fs->unmount();
    }

    delete fs;
    delete bd_mbr;
    bd_raw->deinit();

    return info;
}

float SystemDiagnostics::getCpuLoad()
{
    static mbed_stats_cpu_t prev_stats = {0};
    mbed_stats_cpu_t curr_stats;

    mbed_stats_cpu_get(&curr_stats);

    uint64_t diff_idle = (curr_stats.idle_time - prev_stats.idle_time);
    uint64_t diff_total = (curr_stats.uptime - prev_stats.uptime);

    prev_stats = curr_stats;

    if (diff_total == 0)
        return 0.0f;

    float idle_percent = ((float)diff_idle * 100.0f) / (float)diff_total;
    float usage_percent = 100.0f - idle_percent;

    // Clamp to 0-100
    if (usage_percent < 0)
        usage_percent = 0;
    if (usage_percent > 100)
        usage_percent = 100;

    return usage_percent;
}

void SystemDiagnostics::runOtaDiagnostics()
{
    Serial.println("\n=== OTA Diagnostic Check ===");

    // Check bootloader version
    uint32_t bootloaderVersion = getBootloaderVersion();
    Serial.print("Bootloader version: ");
    Serial.print(bootloaderVersion);
    if (bootloaderVersion >= 22)
    {
        Serial.println(" ✓ (OTA supported)");
    }
    else
    {
        Serial.println(" ✗ (OTA requires >= 22)");
        Serial.println("Update bootloader: File > Examples > Portenta_System > PortentaH7_updateBootloader");
    }

    // Check RTC registers for OTA magic
    bool otaAttempted = checkRtcOtaMagic();

    // Check UPDATE.BIN in QSPI
    if (otaAttempted || bootloaderVersion >= 22)
    {
        checkUpdateBin();
    }

    Serial.println("=== OTA Diagnostic Complete ===\n");
}

uint32_t SystemDiagnostics::getBootloaderVersion()
{
    // Bootloader version stored at specific offset
    uint32_t bootloader_data_offset = 0x1F000;
    uint8_t *bootloader_data = (uint8_t *)(BOOTLOADER_ADDR + bootloader_data_offset);
    return bootloader_data[1];
}

bool SystemDiagnostics::checkRtcOtaMagic()
{
    uint32_t rtc_dr0 = HAL_RTCEx_BKUPRead(&RTCHandle, RTC_BKP_DR0);
    uint32_t rtc_dr1 = HAL_RTCEx_BKUPRead(&RTCHandle, RTC_BKP_DR1);
    uint32_t rtc_dr2 = HAL_RTCEx_BKUPRead(&RTCHandle, RTC_BKP_DR2);
    uint32_t rtc_dr3 = HAL_RTCEx_BKUPRead(&RTCHandle, RTC_BKP_DR3);

    Serial.print("RTC Backup Registers: DR0=0x");
    Serial.print(rtc_dr0, HEX);
    Serial.print(", DR1=");
    Serial.print(rtc_dr1);
    Serial.print(", DR2=");
    Serial.print(rtc_dr2);
    Serial.print(", DR3=");
    Serial.println(rtc_dr3);

    if (rtc_dr0 == 0x07AA)
    {
        Serial.println("WARNING: Bootloader magic 0x07AA still present!");
        Serial.println("This means:");
        Serial.println("  1. OTA was attempted (RTC registers were set)");
        Serial.println("  2. System DID reset (we're running now)");
        Serial.println("  3. Bootloader DID NOT apply the update");
        Serial.println("  Possible causes:");
        Serial.println("  - UPDATE.BIN not found in QSPI");
        Serial.println("  - UPDATE.BIN corrupt or wrong format");
        Serial.println("  - Bootloader cannot read FAT filesystem");

        // Clear the magic to prevent confusion on next boot
        HAL_RTCEx_BKUPWrite(&RTCHandle, RTC_BKP_DR0, 0);
        Serial.println("  Cleared RTC magic to prevent repeat warnings");
        return true;
    }
    else
    {
        Serial.println("No pending OTA detected (RTC magic not set)");
        return false;
    }
}

void SystemDiagnostics::checkUpdateBin()
{
    // Check if UPDATE.BIN exists in QSPI Partition 2
    auto bd_raw = mbed::BlockDevice::get_default_instance();
    if (!bd_raw || bd_raw->init() != 0)
    {
        Serial.println("Cannot access QSPI BlockDevice");
        return;
    }

    auto bd_mbr = new mbed::MBRBlockDevice(bd_raw, 2); // Partition 2
    auto fs = new mbed::FATFileSystem("fs");

    if (fs->mount(bd_mbr) != 0)
    {
        Serial.println("Cannot mount QSPI Partition 2");
        delete fs;
        delete bd_mbr;
        bd_raw->deinit();
        return;
    }

    // Check if UPDATE.BIN exists
    struct stat st;
    if (stat("/fs/UPDATE.BIN", &st) == 0)
    {
        Serial.print("✓ UPDATE.BIN found in QSPI Partition 2! Size: ");
        Serial.print(st.st_size);
        Serial.println(" bytes");

        // Validate firmware format
        FILE *f = fopen("/fs/UPDATE.BIN", "rb");
        if (f)
        {
            uint32_t bootloaderVersion = getBootloaderVersion();

            if (validateFirmwareFormat(f))
            {
                Serial.println("  → Firmware format is CORRECT (raw binary)");
                Serial.println("  → Bootloader SHOULD have found it");

                if (bootloaderVersion >= 22)
                {
                    Serial.println("  → Bootloader version is OK");
                    Serial.println("  → UPDATE.BIN still here = Bootloader may have FAILED to flash it");
                    Serial.println("  → Or bootloader is still processing on next cold boot");
                }
                else
                {
                    Serial.println("  → Bootloader version TOO OLD (< 22) - Cannot flash OTA!");
                }
            }

            fclose(f);
        }
    }
    else
    {
        Serial.println("No UPDATE.BIN in QSPI Partition 2 (normal if no OTA attempted)");
    }

    fs->unmount();
    delete fs;
    delete bd_mbr;
    bd_raw->deinit();
}

bool SystemDiagnostics::validateFirmwareFormat(FILE *file)
{
    uint32_t resetVector[2]; // [stack pointer][reset handler]

    if (fread(resetVector, 4, 2, file) != 2)
    {
        Serial.println("  Format: Cannot read reset vector");
        return false;
    }

    uint32_t stackPointer = resetVector[0];
    uint32_t resetHandler = resetVector[1];

    bool validStackPointer = (stackPointer >= 0x20000000 && stackPointer <= 0x24080000);
    bool validResetHandler = ((resetHandler & 0xFF000000) == 0x08000000) && ((resetHandler & 0x1) == 1);

    Serial.print("  Format: ");
    if (validStackPointer && validResetHandler)
    {
        Serial.println("raw .bin ✓");
        Serial.print("    Stack Pointer: 0x");
        Serial.print(stackPointer, HEX);
        Serial.println(" ✓");
        Serial.print("    Reset Handler: 0x");
        Serial.print(resetHandler, HEX);
        Serial.println(" ✓");
        return true;
    }
    else
    {
        Serial.println("INVALID ✗");
        Serial.print("    Stack Pointer: 0x");
        Serial.print(stackPointer, HEX);
        Serial.println(validStackPointer ? " ✓" : " ✗");
        Serial.print("    Reset Handler: 0x");
        Serial.print(resetHandler, HEX);
        Serial.println(validResetHandler ? " ✓" : " ✗");
        return false;
    }
}
