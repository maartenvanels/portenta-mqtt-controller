#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <stdint.h>
#include <string>
#include <cstring>
#include <math.h>
#include <stdlib.h>

// Basic types
using std::string;
typedef bool boolean;
typedef uint8_t byte;

// String class mock (minimal)
class String : public std::string
{
public:
    String(const char *s) : std::string(s) {}
    String(const std::string &s) : std::string(s) {}
    String(int i) : std::string(std::to_string(i)) {}
    String() : std::string("") {}

    unsigned char operator[](unsigned int index) const
    {
        return std::string::operator[](index);
    }

    void toCharArray(char *buf, unsigned int bufsize, unsigned int index = 0) const
    {
        std::string::copy(buf, bufsize, index);
        buf[length()] = 0; // null terminate
    }

    int toInt() const
    {
        return std::stoi(*this);
    }

    bool isEmpty() const
    {
        return empty();
    }

    int indexOf(char c, int from = 0) const
    {
        size_t pos = find(c, from);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }

    String substring(int from, int to = -1) const
    {
        if (to == -1)
            return substr(from);
        return substr(from, to - from);
    }
};

// PROGMEM compatibility
#define PROGMEM
#define PSTR(s) (s)
inline char *strcpy_P(char *dest, const char *src)
{
    return strcpy(dest, src);
}

// Constants (using const instead of define to avoid conflict with enum class members)
const uint8_t HIGH = 1;
const uint8_t LOW = 0;
const uint8_t INPUT = 0;
const uint8_t OUTPUT = 1;
const uint8_t INPUT_PULLUP = 2;
const uint8_t LSBFIRST = 0;
const uint8_t MSBFIRST = 1;
const uint8_t CHANGE = 1;
const uint8_t FALLING = 2;
const uint8_t RISING = 3;

// Functions
inline unsigned long millis() { return 0; }
inline void delay(unsigned long ms) {}
inline void digitalWrite(uint8_t pin, uint8_t val) {}
inline int digitalRead(uint8_t pin) { return LOW; }
inline void pinMode(uint8_t pin, uint8_t mode) {}
inline long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Serial Mock
class SerialMock
{
public:
    void begin(unsigned long baud) {}
    void print(const char *s)
    {
        printf("%s", s);
        fflush(stdout);
    }
    void print(int n)
    {
        printf("%d", n);
        fflush(stdout);
    }
    void println(const char *s)
    {
        printf("%s\n", s);
        fflush(stdout);
    }
    void println(const String &s)
    {
        printf("%s\n", s.c_str());
        fflush(stdout);
    }
    void println(int n)
    {
        printf("%d\n", n);
        fflush(stdout);
    }
    void println()
    {
        printf("\n");
        fflush(stdout);
    }
};
extern SerialMock Serial;

#endif
