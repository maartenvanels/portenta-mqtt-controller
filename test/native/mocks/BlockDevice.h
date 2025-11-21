#pragma once
namespace mbed {
    class BlockDevice {
    public:
        static BlockDevice* get_default_instance() { return nullptr; }
        virtual int init() { return 0; }
        virtual int deinit() { return 0; }
        virtual int program(const void *buffer, uint64_t addr, uint64_t size) { return 0; }
        virtual int read(void *buffer, uint64_t addr, uint64_t size) { return 0; }
        virtual int erase(uint64_t addr, uint64_t size) { return 0; }
        virtual uint64_t size() { return 1024 * 1024; }
    };
}
