#pragma once
#include <stdio.h>
#include <sys/stat.h>

// Mock functions to replace standard IO in SystemDiagnostics
FILE* test_fopen(const char* filename, const char* mode);
int test_stat(const char* path, struct stat* st);
