#include "Services/Ota/DualBankOTA.h"

// Static member initialization
DualBankOTA::ProgressCallback DualBankOTA::progressCallback_ = nullptr;

// ============================================================================
// PUBLIC API
// ============================================================================

bool DualBankOTA::begin() {
    Serial.println("\n=== Dual-Bank A/B OTA System ===");
    Serial.println("Architecture:");
    Serial.print("  Bootstub: 0x");
    Serial.print(BOOTSTUB_BASE, HEX);
    Serial.print(" (");
    Serial.print(BOOTSTUB_SIZE / 1024);
    Serial.println(" KB)");

    Serial.print("  Slot A:   0x");
    Serial.print(SLOT_A_BASE, HEX);
    Serial.print(" (");
    Serial.print(SLOT_A_SIZE / 1024);
    Serial.println(" KB)");

    Serial.print("  Slot B:   0x");
    Serial.print(SLOT_B_BASE, HEX);
    Serial.print(" (");
    Serial.print(SLOT_B_SIZE / 1024);
    Serial.println(" KB)");

    // Initialize BCB
    if (!initializeBCB()) {
        Serial.println("ERROR: Failed to initialize BCB");
        return false;
    }

    // Enable hardware CRC peripheral
    __HAL_RCC_CRC_CLK_ENABLE();

    BootControlBlock bcb;
    if (!loadBCB(&bcb)) {
        Serial.println("ERROR: Failed to load BCB");
        return false;
    }

    Serial.print("Active Slot: ");
    Serial.println(bcb.active_slot == 0 ? "A" : "B");
    Serial.print("Boot Count: ");
    Serial.println(bcb.boot_count);
    Serial.print("Boot Attempts: ");
    Serial.println(bcb.boot_attempts);

    if (bcb.pending_slot != BCB_NO_PENDING) {
        Serial.print("WARNING: Pending slot detected: ");
        Serial.println(bcb.pending_slot == 0 ? "A" : "B");
        Serial.println("Awaiting boot confirmation via confirmBoot()");
    }

    Serial.println("OTA system ready\n");
    return true;
}

OTAResult DualBankOTA::updateFirmware(const uint8_t* firmwareData, uint32_t firmwareSize, uint32_t version) {
    Serial.println("\n=== Starting Dual-Bank OTA Update ===");
    Serial.print("Firmware size: ");
    Serial.print(firmwareSize);
    Serial.println(" bytes");
    Serial.print("Version: ");
    Serial.println(version);

    // Validate size
    if (firmwareSize == 0 || firmwareSize > SLOT_A_SIZE) {
        Serial.println("✗ Invalid firmware size");
        return OTAResult::ERROR_INVALID_SIZE;
    }

    // Load BCB
    BootControlBlock bcb;
    if (!loadBCB(&bcb)) {
        Serial.println("✗ BCB corrupt");
        return OTAResult::ERROR_BCB_CORRUPT;
    }

    // Determine target slot (inactive one)
    uint8_t targetSlot = getInactiveSlot();
    uint32_t targetAddress = getSlotAddress(targetSlot);

    Serial.print("Target slot: ");
    Serial.println(targetSlot == 0 ? "A" : "B");
    Serial.print("Target address: 0x");
    Serial.println(targetAddress, HEX);

    if (progressCallback_) {
        progressCallback_(0, "Erasing flash...");
    }

    // Step 1: Erase target slot
    Serial.println("\n1. Erasing target slot...");
    if (!eraseSlot(targetSlot)) {
        Serial.println("✗ Flash erase failed");
        return OTAResult::ERROR_FLASH_ERASE;
    }
    Serial.println("✓ Slot erased");

    if (progressCallback_) {
        progressCallback_(10, "Writing firmware...");
    }

    // Step 2: Write firmware (32-byte aligned chunks)
    Serial.println("\n2. Writing firmware to flash...");
    if (!writeFlash(targetAddress, firmwareData, firmwareSize)) {
        Serial.println("✗ Flash write failed");
        return OTAResult::ERROR_FLASH_WRITE;
    }
    Serial.println("✓ Firmware written");

    if (progressCallback_) {
        progressCallback_(80, "Verifying CRC...");
    }

    // Step 3: Verify with hardware CRC
    Serial.println("\n3. Calculating CRC32...");
    uint32_t calculatedCRC = calculateFlashCRC32(targetAddress, firmwareSize);
    uint32_t expectedCRC = calculateCRC32(firmwareData, firmwareSize);

    Serial.print("Expected CRC:   0x");
    Serial.println(expectedCRC, HEX);
    Serial.print("Calculated CRC: 0x");
    Serial.println(calculatedCRC, HEX);

    if (calculatedCRC != expectedCRC) {
        Serial.println("✗ CRC mismatch!");
        return OTAResult::ERROR_CRC_MISMATCH;
    }
    Serial.println("✓ CRC verified");

    if (progressCallback_) {
        progressCallback_(90, "Updating BCB...");
    }

    // Step 4: Update BCB
    Serial.println("\n4. Updating Boot Control Block...");
    if (targetSlot == 0) {
        bcb.slot_a_crc = calculatedCRC;
        bcb.slot_a_size = firmwareSize;
        bcb.slot_a_version = version;
    } else {
        bcb.slot_b_crc = calculatedCRC;
        bcb.slot_b_size = firmwareSize;
        bcb.slot_b_version = version;
    }

    bcb.pending_slot = targetSlot;
    bcb.boot_attempts = 0;

    if (!saveBCB(&bcb)) {
        Serial.println("✗ Failed to save BCB");
        return OTAResult::ERROR_BCB_CORRUPT;
    }
    Serial.println("✓ BCB updated");

    if (progressCallback_) {
        progressCallback_(100, "OTA complete! Rebooting...");
    }

    Serial.println("\n=== OTA Update Complete ===");
    Serial.println("Firmware written to inactive slot");
    Serial.println("BCB.pending_slot set");
    Serial.println("\nRebooting in 3 seconds...");
    Serial.println("New firmware MUST call confirmBoot() within 30 seconds!");

    delay(3000);
    NVIC_SystemReset();

    return OTAResult::SUCCESS;
}

void DualBankOTA::confirmBoot() {
    Serial.println("\n=== Boot Confirmation ===");

    BootControlBlock bcb;
    if (!loadBCB(&bcb)) {
        Serial.println("ERROR: Failed to load BCB");
        return;
    }

    if (bcb.pending_slot == BCB_NO_PENDING) {
        Serial.println("No pending boot to confirm");
        return;
    }

    Serial.print("Confirming boot for slot: ");
    Serial.println(bcb.pending_slot == 0 ? "A" : "B");

    // Promote pending to active
    bcb.active_slot = bcb.pending_slot;
    bcb.pending_slot = BCB_NO_PENDING;
    bcb.boot_attempts = 0;
    bcb.last_boot_ok = millis();

    if (!saveBCB(&bcb)) {
        Serial.println("ERROR: Failed to save BCB");
        return;
    }

    Serial.println("✓ Boot confirmed! Firmware is now active.");
}

bool DualBankOTA::isPendingBootVerification() {
    BootControlBlock bcb;
    if (!loadBCB(&bcb)) {
        return false;
    }
    return (bcb.pending_slot != BCB_NO_PENDING);
}

uint8_t DualBankOTA::getActiveSlot() {
    BootControlBlock bcb;
    if (!loadBCB(&bcb)) {
        return 0; // Default to Slot A
    }
    return bcb.active_slot;
}

uint8_t DualBankOTA::getInactiveSlot() {
    return (getActiveSlot() == 0) ? 1 : 0;
}

void DualBankOTA::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = callback;
}

const char* DualBankOTA::getErrorMessage(OTAResult result) {
    switch (result) {
        case OTAResult::SUCCESS:            return "Success";
        case OTAResult::ERROR_NO_SPACE:     return "No space in target slot";
        case OTAResult::ERROR_FLASH_UNLOCK: return "Failed to unlock flash";
        case OTAResult::ERROR_FLASH_ERASE:  return "Failed to erase flash";
        case OTAResult::ERROR_FLASH_WRITE:  return "Failed to write flash";
        case OTAResult::ERROR_CRC_MISMATCH: return "CRC verification failed";
        case OTAResult::ERROR_INVALID_SIZE: return "Invalid firmware size";
        case OTAResult::ERROR_BCB_CORRUPT:  return "BCB corrupt or unreadable";
        case OTAResult::ERROR_NO_INACTIVE_SLOT: return "No inactive slot available";
        default:                            return "Unknown error";
    }
}

// ============================================================================
// PRIVATE: BCB MANAGEMENT
// ============================================================================

bool DualBankOTA::initializeBCB() {
    // Enable BKPSRAM clock
    __HAL_RCC_BKPRAM_CLK_ENABLE();

    BootControlBlock bcb;
    if (loadBCB(&bcb)) {
        // BCB already valid
        return true;
    }

    // Initialize new BCB
    Serial.println("Initializing new BCB...");
    memset(&bcb, 0, sizeof(bcb));
    bcb.magic = BCB_MAGIC;
    bcb.active_slot = 0;  // Start with Slot A
    bcb.pending_slot = BCB_NO_PENDING;
    bcb.boot_attempts = 0;
    bcb.boot_count = 0;

    return saveBCB(&bcb);
}

bool DualBankOTA::loadBCB(BootControlBlock* bcb) {
    // Read from BKPSRAM
    memcpy(bcb, (void*)BCB_BASE_ADDRESS, sizeof(BootControlBlock));
    return validateBCB(bcb);
}

bool DualBankOTA::saveBCB(const BootControlBlock* bcb) {
    BootControlBlock temp;
    memcpy(&temp, bcb, sizeof(BootControlBlock));

    // Calculate and set CRC
    temp.bcb_crc = calculateBCB_CRC(&temp);

    // Write to BKPSRAM (battery-backed, survives resets)
    memcpy((void*)BCB_BASE_ADDRESS, &temp, sizeof(BootControlBlock));

    // Verify write
    BootControlBlock verify;
    memcpy(&verify, (void*)BCB_BASE_ADDRESS, sizeof(BootControlBlock));

    return (memcmp(&temp, &verify, sizeof(BootControlBlock)) == 0);
}

uint32_t DualBankOTA::calculateBCB_CRC(const BootControlBlock* bcb) {
    // CRC of BCB excluding the bcb_crc field itself
    size_t crcSize = offsetof(BootControlBlock, bcb_crc);
    return calculateCRC32((const uint8_t*)bcb, crcSize);
}

bool DualBankOTA::validateBCB(const BootControlBlock* bcb) {
    if (bcb->magic != BCB_MAGIC) {
        return false;
    }

    uint32_t calculatedCRC = calculateBCB_CRC(bcb);
    return (calculatedCRC == bcb->bcb_crc);
}

// ============================================================================
// PRIVATE: FLASH OPERATIONS (32-byte aligned, interrupts disabled)
// ============================================================================

bool DualBankOTA::eraseSlot(uint8_t slot) {
    uint32_t address = getSlotAddress(slot);
    uint32_t size = getSlotSize(slot);

    // Unlock flash
    HAL_FLASH_Unlock();

    // Find sectors to erase
    // STM32H747 Bank 1: Sector size = 128KB
    uint32_t startSector = (address - FLASH_BASE) / (128 * 1024);
    uint32_t endSector = (address + size - FLASH_BASE) / (128 * 1024);

    Serial.print("Erasing sectors ");
    Serial.print(startSector);
    Serial.print(" to ");
    Serial.println(endSector);

    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.Banks = FLASH_BANK_1;
    eraseInit.Sector = startSector;
    eraseInit.NbSectors = (endSector - startSector) + 1;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    uint32_t sectorError = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&eraseInit, &sectorError);

    HAL_FLASH_Lock();

    if (status != HAL_OK) {
        Serial.print("Erase failed at sector: ");
        Serial.println(sectorError);
        return false;
    }

    return true;
}

bool DualBankOTA::writeFlash(uint32_t address, const uint8_t* data, uint32_t size) {
    // Disable interrupts for atomic flash writes
    __disable_irq();

    HAL_FLASH_Unlock();

    bool success = true;
    uint32_t written = 0;
    uint8_t alignedBuffer[FLASH_WORD_SIZE];

    while (written < size && success) {
        // Prepare 32-byte aligned buffer
        uint32_t remaining = size - written;
        uint32_t chunkSize = (remaining < FLASH_WORD_SIZE) ? remaining : FLASH_WORD_SIZE;

        memset(alignedBuffer, 0xFF, FLASH_WORD_SIZE);
        memcpy(alignedBuffer, data + written, chunkSize);

        // Write 32-byte flash word
        HAL_StatusTypeDef status = HAL_FLASH_Program(
            FLASH_TYPEPROGRAM_FLASHWORD,
            address + written,
            (uint32_t)alignedBuffer
        );

        if (status != HAL_OK) {
            Serial.print("Write failed at 0x");
            Serial.println(address + written, HEX);
            success = false;
            break;
        }

        written += FLASH_WORD_SIZE;

        // Progress callback every 4KB
        if ((written % 4096) == 0 && progressCallback_) {
            uint8_t percent = 10 + ((written * 70) / size);  // 10-80%
            progressCallback_(percent, "Writing...");
        }
    }

    HAL_FLASH_Lock();
    __enable_irq();

    return success;
}

bool DualBankOTA::verifyFlash(uint32_t address, const uint8_t* expected, uint32_t size) {
    uint8_t* flashData = (uint8_t*)address;
    return (memcmp(flashData, expected, size) == 0);
}

// ============================================================================
// PRIVATE: HARDWARE CRC32
// ============================================================================

uint32_t DualBankOTA::calculateCRC32(const uint8_t* data, uint32_t size) {
    // Use STM32 hardware CRC peripheral
    CRC_HandleTypeDef hcrc;
    hcrc.Instance = CRC;
    hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
    hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
    hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
    hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
    hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;

    HAL_CRC_Init(&hcrc);

    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t*)data, size);

    return crc;
}

uint32_t DualBankOTA::calculateFlashCRC32(uint32_t address, uint32_t size) {
    uint8_t* flashData = (uint8_t*)address;
    return calculateCRC32(flashData, size);
}

// ============================================================================
// PRIVATE: HELPERS
// ============================================================================

uint32_t DualBankOTA::getSlotAddress(uint8_t slot) {
    return (slot == 0) ? SLOT_A_BASE : SLOT_B_BASE;
}

uint32_t DualBankOTA::getSlotSize(uint8_t slot) {
    return (slot == 0) ? SLOT_A_SIZE : SLOT_B_SIZE;
}
