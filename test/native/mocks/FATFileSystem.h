#pragma once
#include "BlockDevice.h"

namespace mbed {
    class FATFileSystem {
    public:
        FATFileSystem(const char* name) {}
        int mount(BlockDevice* bd) { return 0; }
        int unmount() { return 0; }
    };
}
