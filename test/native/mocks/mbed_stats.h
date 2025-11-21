#pragma once
#include <stdint.h>

typedef struct {
    uint64_t uptime;
    uint64_t idle_time;
} mbed_stats_cpu_t;

inline void mbed_stats_cpu_get(mbed_stats_cpu_t* stats) {
    stats->uptime = 1000000; // Dummy values
    stats->idle_time = 500000;
}
