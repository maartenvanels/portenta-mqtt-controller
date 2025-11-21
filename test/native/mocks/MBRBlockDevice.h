#pragma once
#include "BlockDevice.h"

namespace mbed {
    class MBRBlockDevice : public BlockDevice {
    public:
        MBRBlockDevice(BlockDevice* bd, int part) {}
    };
}
