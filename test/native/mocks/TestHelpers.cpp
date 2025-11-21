#include "TestHelpers.h"
#include <string.h>
#include <string>

FILE* test_fopen(const char* filename, const char* mode) {
    // Map /fs/UPDATE.BIN to local update.bin
    if (strcmp(filename, "/fs/UPDATE.BIN") == 0) {
        return fopen("update.bin", mode);
    }
    return fopen(filename, mode);
}

int test_stat(const char* path, struct stat* st) {
    if (strcmp(path, "/fs/UPDATE.BIN") == 0) {
        return stat("update.bin", st);
    }
    return stat(path, st);
}
