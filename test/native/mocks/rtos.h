#ifndef MOCK_RTOS_H
#define MOCK_RTOS_H

#include <stdint.h>

namespace rtos {

class Mutex {
public:
    void lock() {}
    void unlock() {}
};

class Thread {
public:
    Thread(int priority = 0, uint32_t stack_size = 0) {}
    void start(void (*task)(void)) {}
};

}

#define osPriorityBelowNormal 0

#endif
