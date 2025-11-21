#ifndef MOCK_UNIFIED_STORAGE_H
#define MOCK_UNIFIED_STORAGE_H

#include <string>
#include <vector>

#define FS_LITTLEFS 0

enum FileMode
{
    READ,
    WRITE,
    APPEND
};

class File
{
public:
    File() : valid(false) {}
    bool exists() { return valid; }
    size_t available() { return content.size(); }
    size_t read(uint8_t *buffer, size_t size) { return 0; }
    int read() { return 0; }
    size_t write(const uint8_t *buffer, size_t size) { return size; }
    void close() {}
    void remove() {}

    bool valid;
    std::string content;
};

class Folder
{
public:
    File createFile(const char *name, FileMode mode)
    {
        return File();
    }
};

class InternalStorage
{
public:
    InternalStorage(int p, const char *n, int fs) {}
    bool begin() { return true; }
    Folder getRootFolder() { return Folder(); }
    bool format(int fs) { return true; }
};

#endif
