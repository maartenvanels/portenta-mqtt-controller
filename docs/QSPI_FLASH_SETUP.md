# QSPI Flash Configuration & Setup

## What is QSPI Flash?

The Arduino Portenta H7 has **two types of memory**:

1.  **Internal Flash (768 KB)**: Located inside the STM32H747 chip. Stores your active Arduino sketch code.
2.  **QSPI Flash (14 MB)**: An external MX25L12833F chip on the board. Stores persistent data, WiFi firmware, and OTA updates.

**QSPI** (Quad Serial Peripheral Interface) is 4x faster than normal SPI because it uses 4 data lines in parallel.

### Memory Map

```
QSPI Flash (14MB total)
├── Partition 1: WiFi (1 MB)
│   ├── 4343WA1.BIN (WiFi firmware)
│   └── TLS Certificates
├── Partition 2: OTA (5 MB)
│   └── UPDATE.BIN (Temporary storage for updates)
├── Partition 3: KVStore (1 MB)
│   └── Key-value storage
└── Partition 4: User Data (7 MB)
    ├── config.json (Application configuration)
    └── logs/ (System logs)
```

## Why Setup is Required

Out of the box, the QSPI flash might not be partitioned correctly for this project. We need specific partitions to ensure:
1.  **WiFi works**: The WiFi chip needs its firmware in Partition 1.
2.  **OTA works**: The bootloader looks for updates in Partition 2.
3.  **Config persistence**: We save `config.json` in Partition 4 so it survives reboots.

## Setup Instructions (One-Time)

### Step 1: Partition QSPI & Install WiFi Firmware

**Via Arduino IDE (one sketch does it all):**

1.  Open Arduino IDE (v4.3.1+ required for Portenta core).
2.  Select board: **Tools > Board > Arduino Portenta H7 (M7 core)**.
3.  Open sketch: **File > Examples > STM32H747_System > QSPIFormat**.
4.  Upload the sketch.
5.  Open Serial Monitor (115200 baud).
6.  Follow the prompts:
    *   `"Do you want to proceed? Y/[n]"` → **Y**
    *   `"Do you want to perform a full erase? Y/[n]"` → **Y** (for fresh start)
    *   `"Partition 1 already contains a filesystem, reformat? Y/[n]"` → **Y**
    *   `"Do you want to restore WiFi firmware and certificates? Y/[n]"` → **Y** (CRITICAL!)
    *   `"Partition 2 already contains a filesystem, reformat? Y/[n]"` → **Y**
    *   `"Use LittleFS for user data? Y/[n]"` → **N** (Use FAT)
    *   `"Partition 4 already contains a filesystem, reformat? Y/[n]"` → **Y**

7.  Wait for: `"QSPI Flash formatted!"` and `"Flashed 100%"`.

### Step 2: Verify

Upload your PlatformIO project and check the serial output. You should see:
```
=== Setting up WiFi Connection ===
WiFi connected!
```

If you see `"File not found - Please run WiFiFirmwareUpdater"`, you missed the "restore WiFi firmware" step. Run QSPIFormat again.

## Safety Warnings

### ⚠️ WiFi Firmware Protection
*   **Partition 1** contains the WiFi driver firmware.
*   **NEVER** call `InternalStorage::restoreDefaultPartitions()` in your code - it wipes everything!
*   **NEVER** format Partition 1 programmatically unless you know what you are doing.

### ✅ Safe Operations
*   Formatting **Partition 2** (OTA) is safe.
*   Formatting **Partition 4** (User Data) is safe (but deletes your config).
*   The OTA system automatically handles Partition 2 safely.

## Troubleshooting

### "QSPI partitions not found"
The board is not partitioned. Run the QSPIFormat sketch (Step 1).

### "WiFi firmware missing"
Partition 1 was wiped. Run the QSPIFormat sketch (Step 1) and answer **Y** to "Restore WiFi firmware".

## References
- [Arduino UnifiedStorage Library](https://github.com/arduino-libraries/Arduino_UnifiedStorage)
- [Portenta H7 Reading/Writing Flash Memory](https://docs.arduino.cc/tutorials/portenta-h7/reading-writing-flash-memory)
