# OTA Firmware Updates

## Overview

This project implements **Over-The-Air (OTA) firmware updates** for the Arduino Portenta H7 using a **hybrid approach** that combines:

- **Custom streaming web upload interface** (for large files >400KB)
- **Arduino Portenta bootloader communication protocol** (proven and reliable)
- **On-the-fly LZSS decompression** (to avoid RAM limitations)

## Quick Start

### 1. Setup OTA with Callbacks

```cpp
#include "OTAUpdate.h"
#include "MQTTManager.h"

// Watchdog feed function (if you have hardware watchdog enabled)
void feedWatchdog() {
    // Reset your watchdog timer here
    // Example: HAL_IWDG_Refresh(&hiwdg);
    Serial.println("Watchdog fed");
}

// Progress callback for MQTT status updates
void otaProgressCallback(uint8_t percent, const char* statusMessage) {
    Serial.print("OTA Progress: ");
    Serial.print(percent);
    Serial.print("% - ");
    Serial.println(statusMessage);

    // Publish to MQTT for monitoring
    MQTTManager::publishOTAProgress(percent, statusMessage);
}

void setup() {
    Serial.begin(115200);

    // Initialize OTA
    OTAUpdate::begin();

    // Register callbacks
    OTAUpdate::setWatchdogCallback(feedWatchdog);
    OTAUpdate::setProgressCallback(otaProgressCallback);

    // Check if OTA is supported
    if (!OTAUpdate::isOtaCapable()) {
        Serial.println("OTA not supported - update bootloader!");
        return;
    }
}
```

### 2. Streaming OTA Update (Recommended)

For web server uploads or large firmwares:

```cpp
// In your web server handler
void handleFirmwareUpload(Client& client, uint32_t contentLength) {
    // Begin streaming update
    if (!OTAUpdate::beginStreamingUpdate(contentLength)) {
        Serial.println("Failed to initialize OTA");
        return;
    }

    // Stream firmware in chunks
    const uint32_t CHUNK_SIZE = 4096;
    uint8_t buffer[CHUNK_SIZE];
    uint32_t bytesRead = 0;

    while (bytesRead < contentLength) {
        size_t chunkSize = client.read(buffer,
            min(CHUNK_SIZE, contentLength - bytesRead));

        if (!OTAUpdate::writeStreamChunk(buffer, chunkSize)) {
            Serial.println("Failed to write chunk");
            OTAUpdate::abortStreamingUpdate();
            return;
        }

        bytesRead += chunkSize;
    }

    // Finalize and reboot
    OTAUpdate::UpdateResult result = OTAUpdate::finalizeStreamingUpdate();

    if (result != OTAUpdate::UPDATE_SUCCESS) {
        Serial.print("OTA failed: ");
        Serial.println(OTAUpdate::getErrorMessage(result));
    }
    // Device will reboot automatically if successful
}
```

## Implementation Details

### Hybrid Approach

Our implementation uses Arduino's proven bootloader communication while adding streaming capabilities:

```cpp
// Our hybrid approach:
OTAUpdate::beginStreamingUpdate(expectedSize);     // Init QSPI + LZSS decoder

// Stream 4KB chunks from HTTP upload
while (receiving) {
    client.read(chunk, 4096);
    OTAUpdate::writeStreamChunk(chunk, size);      // Decompress + write directly to QSPI
}

OTAUpdate::finalizeStreamingUpdate();              // Set RTC registers + reboot (Arduino's method)
```

**Advantages:**
- Streams data in 4KB chunks - never buffers entire file in RAM
- Integrates with our custom web server
- Works behind firewall (no internet access needed)
- On-the-fly LZSS decompression
- Custom progress reporting
- Full control over upload process

### Bootloader Protocol

The Portenta H7 bootloader (version 22+) supports OTA updates via a simple protocol:

1.  **Application** writes parameters to RTC backup registers (Magic value 0x07AA, Partition ID, Size).
2.  **Application** triggers `NVIC_SystemReset()`.
3.  **Bootloader** reads RTC registers.
4.  **Bootloader** flashes `UPDATE.BIN` from QSPI Partition 2 to internal flash.

### QSPI Flash Layout

The Portenta H7 has 14MB of QSPI flash organized with MBR partitioning:

```
QSPI Flash (14MB total)
├── Partition 1: WiFi Firmware (1MB)
├── Partition 2: OTA Storage (5MB) ← We write UPDATE.BIN here
├── Partition 3: KV Store (1MB)
└── Partition 4: User Data (7MB)
```

## Build Process

### 1. Build Firmware

```bash
pio run
```
Output: `.pio/build/portenta_h7_m7/firmware.bin` (~598KB)

### 2. Compress with LZSS

```bash
cd tools
python lzss.py --encode ../.pio/build/portenta_h7_m7/firmware.bin firmware.lzss
```

### 3. Create .ota File

```bash
python bin2ota.py PORTENTA_H7_M7 firmware.lzss firmware.ota
```

### 4. Upload via Web Interface

Navigate to: `http://<device-ip>/` and use the firmware upload form.

## Troubleshooting

### "No partitions found"
**Solution:** Run the QSPIFormat sketch once. See [QSPI_FLASH_SETUP.md](QSPI_FLASH_SETUP.md).

### "Bootloader too old"
**Solution:** Update bootloader via Arduino IDE: `File > Examples > Portenta_System > PortentaH7_updateBootloader`

### "Watchdog timeout"
**Solution:** Ensure `OTAUpdate::setWatchdogCallback` is configured.

## References

- [QSPI Flash Setup](QSPI_FLASH_SETUP.md)
- [Arduino_Portenta_OTA Library](https://github.com/arduino-libraries/Arduino_Portenta_OTA)




