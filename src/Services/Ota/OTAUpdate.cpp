#include "Services/Ota/OTAUpdate.h"
#include <BlockDevice.h>
#include <MBRBlockDevice.h>
#include <FATFileSystem.h>

// Static member initialization
OTAUpdate::WatchdogFeedCallback OTAUpdate::watchdogCallback_ = nullptr;
OTAUpdate::ProgressCallback OTAUpdate::progressCallback_ = nullptr;

// Streaming state initialization
FILE* OTAUpdate::streamFile_ = nullptr;
uint32_t OTAUpdate::streamBytesWritten_ = 0;
uint32_t OTAUpdate::streamExpectedSize_ = 0;
void* OTAUpdate::streamFs_ = nullptr;
void* OTAUpdate::streamBdMbr_ = nullptr;
void* OTAUpdate::streamBdRaw_ = nullptr;

// LZSS decompression state initialization
LZSSDecoder* OTAUpdate::lzssDecoder_ = nullptr;
uint32_t OTAUpdate::streamInputBytesProcessed_ = 0;
uint32_t OTAUpdate::streamOutputBytesWritten_ = 0;
bool OTAUpdate::isOtaFile_ = false;
bool OTAUpdate::formatChecked_ = false;

// External RTC handle (initialized by Arduino framework)
extern RTC_HandleTypeDef RTCHandle;

// Helper function: Check if QSPI partitions exist by reading MBR directly
// This bypasses the InternalStorage::readPartitions() API which has compatibility issues
// Uses structures already defined in Arduino_UnifiedStorage/Partitioning.h
static bool checkQSPIPartitionsViaMBR() {
    auto bd = mbed::BlockDevice::get_default_instance();

    if (bd->init() != 0) {
        Serial.println("✗ Cannot access QSPI flash (hardware issue)");
        return false;
    }

    // Read sector 0 (contains MBR)
    uint8_t buffer[512];
    if (bd->read(buffer, 0, 512) != 0) {
        Serial.println("✗ Failed to read MBR sector");
        return false;
    }

    // Parse MBR table (starts at offset 446)
    mbrTable* table = reinterpret_cast<mbrTable*>(&buffer[446]);

    // Check MBR signature
    if (table->signature[0] != 0x55 || table->signature[1] != 0xAA) {
        Serial.println("✗ Invalid MBR signature - partitions not created");
        return false;
    }

    // Count valid partitions (type != 0x00)
    int partitionCount = 0;
    for (int i = 0; i < 4; i++) {
        if (table->entries[i].type != 0x00) {
            partitionCount++;
        }
    }

    if (partitionCount < 2) {
        Serial.print("✗ Insufficient partitions (found ");
        Serial.print(partitionCount);
        Serial.println(", need at least 2)");
        return false;
    }

    Serial.print("✓ MBR validated: ");
    Serial.print(partitionCount);
    Serial.println(" partitions found");

    return true;
}

void OTAUpdate::begin() {
    Serial.println("\n=== OTA Update System Initialized (Arduino Bootloader Method) ===");
    Serial.println("Firmware will be stored to QSPI flash and applied by bootloader on reboot");
    Serial.print("Max Firmware Size: ");
    Serial.print(MAX_FIRMWARE_SIZE / 1024);
    Serial.println(" KB");
}

bool OTAUpdate::isOtaCapable() {
    // Check bootloader version (same as Arduino_Portenta_OTA)
    // Bootloader data is stored at 0x8000000 + 0x1F000
    // Byte at offset 1 contains the bootloader version
    // OTA is supported starting from version 22
    #define BOOTLOADER_ADDR   (0x8000000)
    uint32_t bootloader_data_offset = 0x1F000;
    uint8_t* bootloader_data = (uint8_t*)(BOOTLOADER_ADDR + bootloader_data_offset);
    uint8_t currentBootloaderVersion = bootloader_data[1];

    Serial.print("Bootloader version: ");
    Serial.println(currentBootloaderVersion);

    if (currentBootloaderVersion < 22) {
        Serial.println("✗ Bootloader version too old for OTA (requires >= 22)");
        Serial.println("Update bootloader: File > Examples > Portenta_System > PortentaH7_updateBootloader");
        return false;
    }

    Serial.println("✓ Bootloader supports OTA");
    return true;
}

bool OTAUpdate::validateFirmwareFormat(const uint8_t* firmwareData, uint32_t firmwareSize) {
    if (firmwareSize < 20 || firmwareSize > MAX_FIRMWARE_SIZE) {
        Serial.println("✗ Invalid firmware size");
        return false;
    }

    // Check if this is an OTA file (Arduino_Portenta_OTA format)
    // OTA format: [4B length][4B CRC32][4B magic][8B version][LZSS data]
    // Magic number for Portenta H7: 0x2341025B
    uint32_t* header = (uint32_t*)firmwareData;
    uint32_t otaMagic = header[2]; // Magic at offset 8

    const uint32_t PORTENTA_H7_MAGIC = 0x2341025B;

    if (otaMagic == PORTENTA_H7_MAGIC) {
        // This is a valid .ota file
        Serial.println("Firmware Validation:");
        Serial.println("  Format: Arduino OTA (.ota)");

        uint32_t otaLength = header[0];
        uint32_t otaCRC = header[1];

        Serial.print("  Length: ");
        Serial.print(otaLength);
        Serial.println(" bytes");
        Serial.print("  CRC32: 0x");
        Serial.println(otaCRC, HEX);
        Serial.print("  Magic: 0x");
        Serial.print(otaMagic, HEX);
        Serial.println(" ✓ (Portenta H7)");

        // Basic sanity check: OTA length should match file size minus 8-byte header
        uint32_t expectedSize = otaLength + 8; // 8 bytes for length+CRC fields
        if (firmwareSize >= expectedSize - 100 && firmwareSize <= expectedSize + 100) {
            Serial.println("  Size check: ✓");
            return true;
        } else {
            Serial.print("  Size mismatch: expected ~");
            Serial.print(expectedSize);
            Serial.print(", got ");
            Serial.println(firmwareSize);
            return false;
        }
    } else {
        // Assume raw .bin format - check ARM Cortex-M7 reset vector
        uint32_t* resetVector = (uint32_t*)firmwareData;
        uint32_t stackPointer = resetVector[0];
        uint32_t resetHandler = resetVector[1];

        // Stack pointer should be in RAM range
        bool validStackPointer = (stackPointer >= RAM_BASE && stackPointer <= RAM_END);

        // Reset handler should be in Flash range and odd (Thumb mode)
        bool validResetHandler = ((resetHandler & 0xFF000000) == 0x08000000) && ((resetHandler & 0x1) == 1);

        Serial.println("Firmware Validation:");
        Serial.println("  Format: Raw binary (.bin)");
        Serial.print("  Stack Pointer: 0x");
        Serial.print(stackPointer, HEX);
        Serial.println(validStackPointer ? " ✓" : " ✗");
        Serial.print("  Reset Handler: 0x");
        Serial.print(resetHandler, HEX);
        Serial.println(validResetHandler ? " ✓" : " ✗");

        return validStackPointer && validResetHandler;
    }
}

OTAUpdate::UpdateResult OTAUpdate::updateFirmware(const uint8_t* firmwareData, uint32_t firmwareSize) {
    Serial.println("\n=== Starting OTA Firmware Update (Bootloader Method) ===");
    Serial.print("Firmware size: ");
    Serial.print(firmwareSize);
    Serial.println(" bytes");

    // Step 1: Validate firmware format
    if (!validateFirmwareFormat(firmwareData, firmwareSize)) {
        Serial.println("✗ Firmware validation failed");
        return UPDATE_ERROR_FORMAT;
    }
    Serial.println("✓ Firmware format validated");

    // Step 2: Write firmware to QSPI flash
    Serial.println("\nWriting firmware to QSPI flash...");
    if (!writeFirmwareToQSPI(firmwareData, firmwareSize)) {
        Serial.println("✗ Failed to write firmware to QSPI flash");
        return UPDATE_ERROR_STORAGE_WRITE;
    }
    Serial.println("✓ Firmware written to QSPI flash");

    // Step 3: Write bootloader parameters to RTC backup registers
    Serial.println("\nConfiguring bootloader parameters...");

    // Detect file format to set correct DR2/DR3
    uint32_t* header = (uint32_t*)firmwareData;
    uint32_t otaMagic = header[2]; // Magic at offset 8
    const uint32_t PORTENTA_H7_MAGIC = 0x2341025B;

    uint32_t dataOffset = 0;
    uint32_t programLength = firmwareSize;

    // CRITICAL: For QSPI_FLASH_FATFS_MBR, DR2 is the PARTITION INDEX, not a data offset!
    dataOffset = 2; // Partition 2

    if (otaMagic == PORTENTA_H7_MAGIC) {
        // .ota format: [4B length][4B CRC32][4B magic][8B version][firmware]
        programLength = firmwareSize - 8;
        Serial.println("Format: .ota (Arduino OTA format)");
    } else {
        // Raw .bin format: [firmware data directly]
        programLength = firmwareSize;
        Serial.println("Format: .bin (raw binary)");
    }

    Serial.print("  DR2 (Partition):    ");
    Serial.println(dataOffset);
    Serial.print("  DR3 (Program Length): ");
    Serial.println(programLength);

    writeBootloaderParams(QSPI_FLASH_FATFS_MBR, dataOffset, programLength);
    Serial.println("✓ Bootloader configured");

    // Step 4: Trigger system reset for bootloader to apply update
    Serial.println("\n=== OTA Update Prepared ===");
    Serial.println("Rebooting now...");
    Serial.println("Bootloader will flash the new firmware from QSPI storage.");
    delay(100); // Allow serial to flush

    triggerBootloaderUpdate();

    return UPDATE_SUCCESS; // This line won't be reached (device will reset)
}

bool OTAUpdate::writeFirmwareToQSPI(const uint8_t* data, uint32_t size) {
    // QSPI Partition Layout (created by QSPIFormat sketch):
    // - Partition 1 (MBR partition 1): WiFi firmware + certs - 1MB
    // - Partition 2 (MBR partition 2): OTA updates - 5MB ← WE USE THIS
    // - Partition 3 (MBR partition 3): KVStore - 1MB
    // - Partition 4 (MBR partition 4): User data - 7MB
    //
    // CRITICAL: We use mbed filesystem API directly (not InternalStorage)
    // to write UPDATE.BIN to /fs/UPDATE.BIN (root of partition 2).
    // This is exactly what Arduino_Portenta_OTA does and what the bootloader expects.

    // Check if partitions exist using direct MBR reading
    Serial.println("Checking QSPI partitions via MBR...");
    if (!checkQSPIPartitionsViaMBR()) {
        Serial.println("✗ QSPI partitions not found!");
        Serial.println("✗ ERROR: Cannot perform OTA update without QSPI partitions.");
        Serial.println("");
        Serial.println("To create partitions (one-time setup):");
        Serial.println("1. Open Arduino IDE");
        Serial.println("2. File > Examples > STM32H747_System > QSPIFormat");
        Serial.println("3. Upload and answer 'Y' to create partitions & install WiFi firmware");
        Serial.println("4. Then retry OTA update");
        Serial.println("");
        Serial.println("NOTE: Auto-partitioning is DISABLED to protect WiFi firmware.");
        return false;

        // NEVER auto-partition! This wipes WiFi firmware in Partition 1:
        // if (!InternalStorage::restoreDefaultPartitions()) { ... }
    }

    // Get QSPI BlockDevice (same as Arduino_Portenta_OTA)
    auto bd_raw = mbed::BlockDevice::get_default_instance();
    if (bd_raw->init() != 0) {
        Serial.println("✗ Failed to initialize QSPI BlockDevice");
        return false;
    }

    // Create MBR BlockDevice for partition 2 (same as bootloader)
    auto bd_mbr = new mbed::MBRBlockDevice(bd_raw, 2);  // MBR partition 2

    // Mount FAT filesystem at "/fs" (same as Arduino_Portenta_OTA)
    auto fs = new mbed::FATFileSystem("fs");
    int mount_result = fs->mount(bd_mbr);

    if (mount_result != 0) {
        Serial.println("✗ Failed to mount partition 2 as FAT");

        // Format ONLY Partition 2 (OTA) - WiFi firmware in Partition 1 stays intact!
        Serial.println("Attempting to format OTA partition (Partition 2)...");
        Serial.println("NOTE: WiFi firmware in Partition 1 will NOT be affected");

        if (fs->reformat(bd_mbr) != 0) {
            Serial.println("✗ Failed to format OTA partition");
            delete fs;
            delete bd_mbr;
            bd_raw->deinit();
            return false;
        }

        Serial.println("✓ OTA partition formatted (WiFi firmware safe)");

        // Try again
        if (fs->mount(bd_mbr) != 0) {
            Serial.println("✗ Failed to mount OTA partition after format");
            delete fs;
            delete bd_mbr;
            bd_raw->deinit();
            return false;
        }
    }

    Serial.println("✓ Partition 2 mounted at /fs");

    // Write UPDATE.BIN to /fs/UPDATE.BIN (root of partition 2, where bootloader expects it)
    const char* filepath = "/fs/UPDATE.BIN";
    Serial.print("Writing ");
    Serial.print(size);
    Serial.print(" bytes to ");
    Serial.println(filepath);

    FILE* file = fopen(filepath, "wb");
    if (!file) {
        Serial.println("✗ Failed to create UPDATE.BIN on QSPI");
        fs->unmount();
        delete fs;
        delete bd_mbr;
        bd_raw->deinit();
        return false;
    }

    // Write firmware data in chunks
    const uint32_t chunkSize = 4096; // 4KB chunks
    uint32_t bytesWritten = 0;
    uint32_t lastProgress = 0;
    uint32_t lastWatchdogFeed = 0;
    const uint32_t WATCHDOG_FEED_INTERVAL = 32768; // Feed watchdog every 32KB

    while (bytesWritten < size) {
        uint32_t remainingBytes = size - bytesWritten;
        uint32_t bytesToWrite = (remainingBytes < chunkSize) ? remainingBytes : chunkSize;

        size_t written = fwrite(data + bytesWritten, 1, bytesToWrite, file);

        if (written != bytesToWrite) {
            Serial.print("✗ Write error at offset ");
            Serial.print(bytesWritten);
            Serial.print(": wrote ");
            Serial.print(written);
            Serial.print(" of ");
            Serial.println(bytesToWrite);

            if (progressCallback_ != nullptr) {
                progressCallback_(0, "Write error");
            }

            fclose(file);
            fs->unmount();
            delete fs;
            delete bd_mbr;
            bd_raw->deinit();
            return false;
        }

        bytesWritten += written;

        // Feed watchdog every 32KB to prevent timeout during long writes
        if (bytesWritten - lastWatchdogFeed >= WATCHDOG_FEED_INTERVAL) {
            feedWatchdog();
            lastWatchdogFeed = bytesWritten;
        }

        // Print progress every 10%
        uint32_t progress = (bytesWritten * 100) / size;
        if (progress >= lastProgress + 10) {
            Serial.print("Writing: ");
            Serial.print(progress);
            Serial.println("%");
            lastProgress = progress;

            // Invoke progress callback with detailed message
            if (progressCallback_ != nullptr) {
                char statusMsg[32];
                snprintf(statusMsg, sizeof(statusMsg), "Writing: %lu/%lu bytes", bytesWritten, size);
                progressCallback_((uint8_t)progress, statusMsg);
            }
        }
    }

    // Close and sync file
    fclose(file);
    Serial.println("✓ Firmware file written successfully");

    // Unmount filesystem to ensure all data is flushed to QSPI
    Serial.println("Syncing filesystem...");
    fs->unmount();

    Serial.print("✓ Wrote ");
    Serial.print(bytesWritten);
    Serial.print(" bytes to ");
    Serial.println(filepath);
    Serial.println("✓ UPDATE.BIN ready at /fs/UPDATE.BIN (bootloader will find it!)");

    // Cleanup
    delete fs;
    delete bd_mbr;
    bd_raw->deinit();

    return true;
}

void OTAUpdate::writeBootloaderParams(uint32_t storageType, uint32_t dataOffset, uint32_t programLength) {
    // Write OTA parameters to RTC backup registers (same as Arduino_Portenta_OTA)
    // The bootloader reads these registers on boot to determine if OTA update is pending

    HAL_RTCEx_BKUPWrite(&RTCHandle, RTC_BKP_DR0, BOOTLOADER_MAGIC);  // Magic value 0x07AA
    HAL_RTCEx_BKUPWrite(&RTCHandle, RTC_BKP_DR1, storageType);       // QSPI_FLASH_FATFS (36) - Bootloader might expect this even for LittleFS if it supports it, or we might need a different constant. 
                                                                     // Assuming bootloader supports LittleFS if user requested it, or this is just a storage ID.
                                                                     // If bootloader ONLY supports FAT, this change will break OTA.
                                                                     // However, user explicitly asked for LittleFS.
    HAL_RTCEx_BKUPWrite(&RTCHandle, RTC_BKP_DR2, dataOffset);        // 0 (start of file)
    HAL_RTCEx_BKUPWrite(&RTCHandle, RTC_BKP_DR3, programLength);     // Firmware size

    Serial.println("RTC Backup Registers:");
    Serial.print("  DR0 (Magic):        0x");
    Serial.println(BOOTLOADER_MAGIC, HEX);
    Serial.print("  DR1 (Storage Type): ");
    Serial.println(storageType);
    Serial.print("  DR2 (Data Offset):  ");
    Serial.println(dataOffset);
    Serial.print("  DR3 (Prog Length):  ");
    Serial.println(programLength);
}

void OTAUpdate::triggerBootloaderUpdate() {
    // Trigger system reset - bootloader will detect magic value and flash firmware
    // Use exact same reset method as Arduino_Portenta_OTA library
    NVIC_SystemReset();
}

const char* OTAUpdate::getErrorMessage(UpdateResult result) {
    switch (result) {
        case UPDATE_SUCCESS:
            return "Update successful";
        case UPDATE_ERROR_SIZE:
            return "Firmware size invalid";
        case UPDATE_ERROR_FORMAT:
            return "Firmware format invalid";
        case UPDATE_ERROR_STORAGE_INIT:
            return "Failed to initialize QSPI storage";
        case UPDATE_ERROR_STORAGE_WRITE:
            return "Failed to write to QSPI storage";
        case UPDATE_ERROR_RTC:
            return "Failed to configure bootloader";
        case UPDATE_ERROR_BOOTLOADER_VERSION:
            return "Bootloader version too old (requires v22+)";
        case UPDATE_ERROR_NO_PARTITIONS:
            return "QSPI partitions not found - run QSPIFormat";
        case UPDATE_ERROR_MOUNT_FAILED:
            return "Failed to mount OTA partition";
        case UPDATE_ERROR_FILE_CREATE:
            return "Failed to create UPDATE.BIN file";
        case UPDATE_ERROR_DECOMPRESS:
            return "LZSS decompression failed";
        case UPDATE_ERROR_SIZE_MISMATCH:
            return "Firmware size mismatch";
        default:
            return "Unknown error";
    }
}

// Watchdog and Progress Callback Setters
void OTAUpdate::setWatchdogCallback(WatchdogFeedCallback callback) {
    watchdogCallback_ = callback;
    Serial.println("✓ Watchdog callback registered");
}

void OTAUpdate::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = callback;
    Serial.println("✓ Progress callback registered");
}

void OTAUpdate::feedWatchdog() {
    if (watchdogCallback_ != nullptr) {
        watchdogCallback_();
    }
}

// Streaming API Implementation

bool OTAUpdate::beginStreamingUpdate(uint32_t expectedSize) {
    Serial.println("\n=== Beginning Streaming OTA Update ===");
    Serial.print("Expected firmware size: ");
    Serial.print(expectedSize);
    Serial.println(" bytes");

    if (progressCallback_ != nullptr) {
        progressCallback_(0, "Initializing OTA");
    }

    // Check bootloader version
    if (!isOtaCapable()) {
        if (progressCallback_ != nullptr) {
            progressCallback_(0, "Bootloader too old");
        }
        return false;
    }

    // Validate size
    if (expectedSize == 0 || expectedSize > MAX_FIRMWARE_SIZE) {
        Serial.println("✗ Invalid firmware size");
        if (progressCallback_ != nullptr) {
            progressCallback_(0, "Invalid size");
        }
        return false;
    }

    // Check if partitions exist
    Serial.println("Checking QSPI partitions via MBR...");
    if (progressCallback_ != nullptr) {
        progressCallback_(1, "Checking partitions");
    }

    if (!checkQSPIPartitionsViaMBR()) {
        Serial.println("✗ QSPI partitions not found!");
        if (progressCallback_ != nullptr) {
            progressCallback_(0, "No partitions - run QSPIFormat");
        }
        return false;
    }

    // Get QSPI BlockDevice
    if (progressCallback_ != nullptr) {
        progressCallback_(2, "Initializing QSPI");
    }

    auto bd_raw = mbed::BlockDevice::get_default_instance();
    if (bd_raw->init() != 0) {
        Serial.println("✗ Failed to initialize QSPI BlockDevice");
        if (progressCallback_ != nullptr) {
            progressCallback_(0, "QSPI init failed");
        }
        return false;
    }

    // Create MBR BlockDevice for partition 2
    auto bd_mbr = new mbed::MBRBlockDevice(bd_raw, 2);

    // Mount FAT filesystem at "/fs"
    if (progressCallback_ != nullptr) {
        progressCallback_(3, "Mounting partition");
    }

    auto fs = new mbed::FATFileSystem("fs");
    int mount_result = fs->mount(bd_mbr);

    if (mount_result != 0) {
        Serial.println("✗ Failed to mount partition 2 as FAT");
        Serial.println("✗ ERROR: OTA partition not formatted!");
        Serial.println("");
        Serial.println("SOLUTION: Run QSPIFormat sketch (ONE-TIME SETUP):");
        Serial.println("  1. Open Arduino IDE");
        Serial.println("  2. File > Examples > STM32H747_System > QSPIFormat");
        Serial.println("  3. Upload sketch to Portenta H7");
        Serial.println("  4. Open Serial Monitor");
        Serial.println("  5. Type 'Y' when prompted to create partitions");
        Serial.println("  6. Wait for completion (creates 4 partitions + WiFi firmware)");
        Serial.println("  7. Then retry OTA update");
        Serial.println("");
        Serial.println("Note: This only needs to be done ONCE per device");

        if (progressCallback_ != nullptr) {
            progressCallback_(0, "Run QSPIFormat first");
        }

        delete fs;
        delete bd_mbr;
        bd_raw->deinit();
        return false;
    }

    Serial.println("✓ Partition 2 mounted at /fs");

    // Open UPDATE.BIN for writing (same as Arduino_Portenta_OTA)
    if (progressCallback_ != nullptr) {
        progressCallback_(5, "Creating UPDATE.BIN");
    }

    const char* filepath = "/fs/UPDATE.BIN";
    FILE* file = fopen(filepath, "wb");
    if (!file) {
        Serial.println("✗ Failed to create UPDATE.BIN on QSPI");
        if (progressCallback_ != nullptr) {
            progressCallback_(0, "File create failed");
        }
        fs->unmount();
        delete fs;
        delete bd_mbr;
        bd_raw->deinit();
        return false;
    }

    Serial.println("✓ UPDATE.BIN opened for streaming");

    // Initialize LZSS decoder with callback to write to file
    if (lzssDecoder_) {
        delete lzssDecoder_;
    }

    lzssDecoder_ = new LZSSDecoder([file](const uint8_t byte) {
        fwrite(&byte, 1, 1, file);
    });

    Serial.println("✓ LZSS decoder initialized");
    if (progressCallback_ != nullptr) {
        progressCallback_(5, "Ready for upload");
    }

    // Save state
    streamFile_ = file;
    streamBytesWritten_ = 0;
    streamExpectedSize_ = expectedSize;
    streamInputBytesProcessed_ = 0;
    streamOutputBytesWritten_ = 0;
    isOtaFile_ = false;
    formatChecked_ = false;
    streamFs_ = (void*)fs;
    streamBdMbr_ = (void*)bd_mbr;
    streamBdRaw_ = (void*)bd_raw;

    return true;
}

bool OTAUpdate::writeStreamChunk(const uint8_t* data, uint32_t size) {
    if (!streamFile_ || !lzssDecoder_) {
        Serial.println("✗ Stream not initialized");
        return false;
    }

    // Format detection on first chunk
    if (!formatChecked_) {
        if (streamInputBytesProcessed_ == 0 && size >= 12) {
            uint32_t* header = (uint32_t*)data;
            uint32_t magic = header[2]; // Magic at offset 8
            const uint32_t PORTENTA_H7_MAGIC = 0x2341025B;

            if (magic == PORTENTA_H7_MAGIC) {
                isOtaFile_ = true;
                Serial.println("Stream Format: .ota (Compressed)");
            } else {
                isOtaFile_ = false;
                Serial.println("Stream Format: .bin (Raw Binary)");
            }
            formatChecked_ = true;
        } else {
            // Fallback if chunk is too small (unlikely) or not first chunk
            if (streamInputBytesProcessed_ == 0) {
                 Serial.println("Stream Warning: First chunk too small for magic check, assuming Raw Binary");
                 isOtaFile_ = false;
                 formatChecked_ = true;
            }
        }
    }

    if (!isOtaFile_ && formatChecked_) {
        // RAW BINARY: Write directly to file, bypassing LZSS and header skipping
        fwrite(data, 1, size, streamFile_);
        streamBytesWritten_ += size;
        streamInputBytesProcessed_ += size;
    } else {
        // .OTA FILE: Use existing logic (Header Skip + LZSS)
        const uint8_t* processData = data;
        uint32_t processSize = size;

        // Skip OTA header (first 20 bytes) if we haven't processed any data yet
        if (streamInputBytesProcessed_ < OTA_HEADER_SIZE) {
            uint32_t headerBytesToSkip = OTA_HEADER_SIZE - streamInputBytesProcessed_;

            if (headerBytesToSkip >= size) {
                // Entire chunk is header, skip it all
                streamInputBytesProcessed_ += size;
                streamBytesWritten_ += size;
                return true;
            }

            // Skip partial header
            processData += headerBytesToSkip;
            processSize -= headerBytesToSkip;
            streamInputBytesProcessed_ += headerBytesToSkip;
        }

        // Decompress LZSS data
        if (processSize > 0) {
            // Feed data to LZSS decoder (it will call our callback to write decompressed bytes)
            // decompress() returns:
            //   DONE: decompression completed
            //   IN_PROGRESS: cycle completed successfully, ready for next chunk
            //   NOT_COMPLETED: insufficient data (not an error, just needs more data)
            lzssDecoder_->decompress((uint8_t*)processData, processSize);

            streamInputBytesProcessed_ += processSize;
            streamBytesWritten_ += size;  // Total input bytes received

            // All status values are valid during streaming
            // We continue feeding data until the entire file is processed
        }
    }

    // Feed watchdog every 32KB to prevent timeout during long operations
    static uint32_t lastWatchdogFeed = 0;
    const uint32_t WATCHDOG_FEED_INTERVAL = 32768; // 32KB

    if (streamBytesWritten_ - lastWatchdogFeed >= WATCHDOG_FEED_INTERVAL) {
        feedWatchdog();
        lastWatchdogFeed = streamBytesWritten_;
    }

    // Print progress every 10%
    static uint32_t lastProgress = 0;
    uint32_t progress = (streamBytesWritten_ * 100) / streamExpectedSize_;
    if (progress >= lastProgress + 10) {
        Serial.print("Uploading: ");
        Serial.print(progress);
        Serial.println("%");
        lastProgress = progress;

        // Invoke progress callback with detailed message
        if (progressCallback_ != nullptr) {
            char statusMsg[48];
            snprintf(statusMsg, sizeof(statusMsg), "Uploading: %lu/%lu KB",
                     streamBytesWritten_ / 1024, streamExpectedSize_ / 1024);
            progressCallback_((uint8_t)progress, statusMsg);
        }
    }

    return true;
}

OTAUpdate::UpdateResult OTAUpdate::finalizeStreamingUpdate() {
    if (!streamFile_) {
        Serial.println("✗ Stream not initialized");
        if (progressCallback_ != nullptr) {
            progressCallback_(0, "Stream not initialized");
        }
        return UPDATE_ERROR_STORAGE_WRITE;
    }

    if (progressCallback_ != nullptr) {
        progressCallback_(95, "Finalizing upload");
    }

    // Clean up LZSS decoder
    if (lzssDecoder_) {
        delete lzssDecoder_;
        lzssDecoder_ = nullptr;
    }

    // Close file
    fclose(streamFile_);
    Serial.println("✓ Firmware file written successfully");

    // Check if we got all the input bytes
    if (streamBytesWritten_ != streamExpectedSize_) {
        Serial.print("✗ Size mismatch: expected ");
        Serial.print(streamExpectedSize_);
        Serial.print(", received ");
        Serial.println(streamBytesWritten_);

        if (progressCallback_ != nullptr) {
            progressCallback_(0, "Size mismatch");
        }

        // Clean up
        auto fs = (mbed::FATFileSystem*)streamFs_;
        auto bd_mbr = (mbed::MBRBlockDevice*)streamBdMbr_;
        auto bd_raw = (mbed::BlockDevice*)streamBdRaw_;

        fs->unmount();
        delete fs;
        delete bd_mbr;
        bd_raw->deinit();

        streamFile_ = nullptr;
        return UPDATE_ERROR_SIZE_MISMATCH;
    }

    Serial.print("✓ Received ");
    Serial.print(streamBytesWritten_);
    Serial.println(" bytes (compressed .ota)");

    if (progressCallback_ != nullptr) {
        progressCallback_(96, "Verifying firmware");
    }

    // Get actual decompressed file size
    struct stat st;
    if (stat("/fs/UPDATE.BIN", &st) != 0) {
        Serial.println("✗ Failed to stat UPDATE.BIN");

        if (progressCallback_ != nullptr) {
            progressCallback_(0, "Verification failed");
        }

        auto fs = (mbed::FATFileSystem*)streamFs_;
        auto bd_mbr = (mbed::MBRBlockDevice*)streamBdMbr_;
        auto bd_raw = (mbed::BlockDevice*)streamBdRaw_;
        fs->unmount();
        delete fs;
        delete bd_mbr;
        bd_raw->deinit();
        streamFile_ = nullptr;
        return UPDATE_ERROR_STORAGE_WRITE;
    }

    uint32_t decompressedSize = st.st_size;

    Serial.print("✓ Decompressed to ");
    Serial.print(decompressedSize);
    Serial.println(" bytes (raw .bin)");

    // UPDATE.BIN now contains RAW BINARY (decompressed), not .ota format
    // Bootloader parameters (same as Arduino_Portenta_OTA):
    // DR1 = QSPI_FLASH_FATFS_MBR (164) - storage type with MBR flag
    // DR2 = 2 - PARTITION NUMBER (not file offset!)
    // DR3 = decompressed file size
    uint32_t dataOffset = 2;  // Partition 2 (same as Arduino_Portenta_OTA)
    uint32_t programLength = decompressedSize;  // Decompressed firmware size

    Serial.println("\nFirmware format: raw .bin (LZSS decompressed)");
    Serial.print("  DR2 (Partition):  ");
    Serial.println(dataOffset);
    Serial.print("  DR3 (Program Length): ");
    Serial.println(programLength);

    if (progressCallback_ != nullptr) {
        progressCallback_(97, "Syncing filesystem");
    }

    // DO NOT unmount filesystem - keep it open like Arduino_Portenta_OTA does!
    // The bootloader may need the filesystem to remain mounted
    Serial.println("\nSyncing filesystem...");
    // Unused variable: auto fs = (mbed::FATFileSystem*)streamFs_;

    // Just ensure data is flushed (fsync equivalent)
    fflush(NULL);
    Serial.println("✓ UPDATE.BIN ready at /fs/UPDATE.BIN (filesystem still mounted)");

    // Clear streaming state (but keep FS objects alive!)
    streamFile_ = nullptr;

    if (progressCallback_ != nullptr) {
        progressCallback_(98, "Configuring bootloader");
    }

    // Configure bootloader parameters
    Serial.println("\nConfiguring bootloader parameters...");
    writeBootloaderParams(QSPI_FLASH_FATFS_MBR, dataOffset, programLength);
    Serial.println("✓ Bootloader configured");

    if (progressCallback_ != nullptr) {
        progressCallback_(100, "Rebooting");
    }

    // Trigger system reset WITHOUT unmounting (Arduino_Portenta_OTA also doesn't unmount)
    Serial.println("\n=== OTA Update Prepared ===");
    Serial.println("Rebooting now...");
    Serial.println("Bootloader will flash the new firmware from QSPI storage.");
    Serial.println("NOTE: Filesystem left mounted for bootloader access");
    delay(100);

    triggerBootloaderUpdate();

    // This code won't be reached, but if it were, we'd clean up:
    // fs->unmount();
    // delete fs;
    // delete bd_mbr;
    // bd_raw->deinit();

    return UPDATE_SUCCESS;
}

void OTAUpdate::abortStreamingUpdate() {
    if (!streamFile_) {
        return;  // Nothing to abort
    }

    Serial.println("\n✗ Aborting streaming update");

    // Clean up LZSS decoder
    if (lzssDecoder_) {
        delete lzssDecoder_;
        lzssDecoder_ = nullptr;
    }

    // Close file
    fclose(streamFile_);

    // Unmount and cleanup
    auto fs = (mbed::FATFileSystem*)streamFs_;
    auto bd_mbr = (mbed::MBRBlockDevice*)streamBdMbr_;
    auto bd_raw = (mbed::BlockDevice*)streamBdRaw_;

    fs->unmount();
    delete fs;
    delete bd_mbr;
    bd_raw->deinit();

    // Clear state
    streamFile_ = nullptr;
    streamBytesWritten_ = 0;
    streamExpectedSize_ = 0;
    streamInputBytesProcessed_ = 0;
    streamOutputBytesWritten_ = 0;
    streamFs_ = nullptr;
    streamBdMbr_ = nullptr;
    streamBdRaw_ = nullptr;

    Serial.println("✓ Stream aborted and resources cleaned up");
}
