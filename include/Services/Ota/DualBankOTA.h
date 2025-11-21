#ifndef DUAL_BANK_OTA_H
#define DUAL_BANK_OTA_H

#include <Arduino.h>
#include "stm32h7xx_hal.h"

// ============================================================================
// DUAL-BANK A/B OTA IMPLEMENTATION
// ============================================================================
// Architecture:
// - Bootstub (8KB @ 0x08000000) - NEVER UPDATED
// - Slot A (380KB @ 0x08002000) - App firmware bank A
// - Slot B (380KB @ 0x08062000) - App firmware bank B
// - BCB in BKPSRAM (64 bytes @ 0x38800000)
//
// OTA Flow:
// 1. Download firmware → RAM buffer
// 2. Direct 32-byte aligned flash writes to inactive slot
// 3. Hardware CRC32 verification
// 4. Set BCB.pending_slot → Reset
// 5. New firmware sends "boot_ok" via MQTT within timeout
// 6. BCB promotes pending → active (or rollback on failure)
// ============================================================================

// Memory Layout (STM32H747 - 2MB Flash)
#define BOOTSTUB_BASE       0x08000000UL    // Sector 0 (8KB) - Never updated
#define BOOTSTUB_SIZE       0x00002000UL    // 8KB

#define SLOT_A_BASE         0x08002000UL    // Starts after bootstub
#define SLOT_A_SIZE         0x0005F000UL    // 380KB (enough for our 591KB compressed)

#define SLOT_B_BASE         0x08062000UL    // After Slot A
#define SLOT_B_SIZE         0x0005F000UL    // 380KB

// BKPSRAM (Battery-backed SRAM - survives resets)
#define BCB_BASE_ADDRESS    0x38800000UL    // BKPSRAM base
#define BCB_SIZE            64              // 64 bytes for BCB

// Flash programming alignment (H7 requires 256-bit = 32 bytes)
#define FLASH_WORD_SIZE     32

// Boot Control Block (BCB) Structure
// Stored in BKPSRAM to survive resets
typedef struct __attribute__((packed)) {
    uint32_t magic;             // 0xB00TCAFE - BCB validity marker
    uint8_t  active_slot;       // 0 = Slot A, 1 = Slot B
    uint8_t  pending_slot;      // 0xFF = none, 0 = A, 1 = B
    uint8_t  boot_attempts;     // Number of boot attempts (rollback if > max)
    uint8_t  reserved;

    uint32_t slot_a_crc;        // CRC32 of Slot A firmware
    uint32_t slot_a_size;       // Size of Slot A firmware
    uint32_t slot_a_version;    // Slot A firmware version

    uint32_t slot_b_crc;        // CRC32 of Slot B firmware
    uint32_t slot_b_size;       // Size of Slot B firmware
    uint32_t slot_b_version;    // Slot B firmware version

    uint32_t boot_count;        // Total boot counter
    uint32_t last_boot_ok;      // millis() of last successful boot_ok

    uint8_t  padding[16];       // Reserve for future use
    uint32_t bcb_crc;           // CRC32 of entire BCB (excluding this field)
} BootControlBlock;

#define BCB_MAGIC           0xB007CAFE
#define BCB_NO_PENDING      0xFF
#define BCB_MAX_BOOT_ATTEMPTS   3

// OTA Result Codes
enum class OTAResult {
    SUCCESS = 0,
    ERROR_NO_SPACE = -1,
    ERROR_FLASH_UNLOCK = -2,
    ERROR_FLASH_ERASE = -3,
    ERROR_FLASH_WRITE = -4,
    ERROR_CRC_MISMATCH = -5,
    ERROR_INVALID_SIZE = -6,
    ERROR_BCB_CORRUPT = -7,
    ERROR_NO_INACTIVE_SLOT = -8
};

// Boot Verification Status
enum class BootStatus {
    PENDING,        // Waiting for boot_ok
    VERIFIED,       // boot_ok received, promoted to active
    ROLLBACK        // Timeout or failure, rolled back
};

class DualBankOTA {
public:
    // Initialize OTA system and BCB
    static bool begin();

    // Main OTA update function
    // Downloads to RAM, writes to inactive slot, verifies CRC
    static OTAResult updateFirmware(const uint8_t* firmwareData, uint32_t firmwareSize, uint32_t version);

    // Call this from app after successful boot to confirm new firmware works
    static void confirmBoot();

    // Check if we're in pending boot verification state
    static bool isPendingBootVerification();

    // Get current active slot
    static uint8_t getActiveSlot();

    // Get inactive slot (target for OTA)
    static uint8_t getInactiveSlot();

    // Progress callback for MQTT updates during flash write
    typedef void (*ProgressCallback)(uint8_t percent, const char* status);
    static void setProgressCallback(ProgressCallback callback);

    // Get human-readable error message
    static const char* getErrorMessage(OTAResult result);

private:
    // BCB Management
    static bool initializeBCB();
    static bool loadBCB(BootControlBlock* bcb);
    static bool saveBCB(const BootControlBlock* bcb);
    static uint32_t calculateBCB_CRC(const BootControlBlock* bcb);
    static bool validateBCB(const BootControlBlock* bcb);

    // Flash Operations (32-byte aligned, interrupts disabled)
    static bool eraseSlot(uint8_t slot);
    static bool writeFlash(uint32_t address, const uint8_t* data, uint32_t size);
    static bool verifyFlash(uint32_t address, const uint8_t* expected, uint32_t size);

    // CRC using STM32 hardware CRC peripheral
    static uint32_t calculateCRC32(const uint8_t* data, uint32_t size);
    static uint32_t calculateFlashCRC32(uint32_t address, uint32_t size);

    // Slot address helpers
    static uint32_t getSlotAddress(uint8_t slot);
    static uint32_t getSlotSize(uint8_t slot);

    // Progress callback
    static ProgressCallback progressCallback_;

    // Boot verification timeout (30 seconds)
    static constexpr uint32_t BOOT_VERIFY_TIMEOUT_MS = 30000;
};

#endif // DUAL_BANK_OTA_H
