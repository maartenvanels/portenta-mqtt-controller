#include <unity.h>
#define private public
#include "Core/SystemDiagnostics.h"
#include <stdio.h>

void create_valid_firmware_file() {
    FILE* f = fopen("update.bin", "wb");
    if (f) {
        // Valid Stack Pointer: 0x20001000 (RAM)
        uint32_t sp = 0x20001000;
        // Valid Reset Handler: 0x08000401 (Flash, odd for Thumb)
        uint32_t rh = 0x08000401;
        
        fwrite(&sp, 4, 1, f);
        fwrite(&rh, 4, 1, f);
        
        // Fill some dummy data
        for(int i=0; i<100; i++) {
            uint8_t b = 0;
            fwrite(&b, 1, 1, f);
        }
        fclose(f);
    }
}

void create_invalid_firmware_file() {
    FILE* f = fopen("update.bin", "wb");
    if (f) {
        // Invalid Stack Pointer: 0x00000000
        uint32_t sp = 0x00000000;
        uint32_t rh = 0x08000401;
        
        fwrite(&sp, 4, 1, f);
        fwrite(&rh, 4, 1, f);
        fclose(f);
    }
}

// Access private method via friend or helper? 
// Since we can't easily modify the class to add friend without changing source,
// we will test checkUpdateBin() which calls validateFirmwareFormat.
// But checkUpdateBin prints to Serial and does other checks.
// Alternatively, we can use the preprocessor to make private public for testing.
// #define private public before including the header?
// That's a hack but works for tests.

void test_firmware_validation_valid() {
    create_valid_firmware_file();
    
    FILE* f = fopen("update.bin", "rb");
    TEST_ASSERT_NOT_NULL(f);
    
    // We need to call the static private method.
    // Since we are in a test, we can't easily access private.
    // Let's use the hack.
    
    bool result = SystemDiagnostics::validateFirmwareFormat(f);
    TEST_ASSERT_TRUE(result);
    
    fclose(f);
}

void test_firmware_validation_invalid() {
    create_invalid_firmware_file();
    
    FILE* f = fopen("update.bin", "rb");
    TEST_ASSERT_NOT_NULL(f);
    
    bool result = SystemDiagnostics::validateFirmwareFormat(f);
    TEST_ASSERT_FALSE(result);
    
    fclose(f);
}
