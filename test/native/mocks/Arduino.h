#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <stdint.h>
#include <string>
#include <cstring>
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <type_traits>

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
    String(float f, int decimals)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimals, f);
        assign(buf);
    }
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

// Constants for Serial formatting
const uint8_t HEX = 16;

// IPAddress Mock (needed by SerialMock)
struct IPAddress
{
    uint8_t bytes[4];
    IPAddress()
    {
        bytes[0] = 0;
        bytes[1] = 0;
        bytes[2] = 0;
        bytes[3] = 0;
    }
    IPAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
    {
        bytes[0] = b1;
        bytes[1] = b2;
        bytes[2] = b3;
        bytes[3] = b4;
    }
    bool operator!=(const IPAddress &other) const
    {
        return bytes[0] != other.bytes[0] || bytes[1] != other.bytes[1] ||
               bytes[2] != other.bytes[2] || bytes[3] != other.bytes[3];
    }
    bool operator==(const IPAddress &other) const
    {
        return !(*this != other);
    }
    String toString() const
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
        return String(buf);
    }
};

// Serial Mock
class SerialMock
{
public:
    void begin(unsigned long baud) {}

    void print(const char *s)
    {
        std::cout << s;
    }

    void print(const String &s)
    {
        std::cout << s;
    }

    void print(IPAddress ip)
    {
        std::cout << (int)ip.bytes[0] << "." << (int)ip.bytes[1] << "."
                  << (int)ip.bytes[2] << "." << (int)ip.bytes[3];
    }

    // Template for all numeric types to avoid ambiguity
    template <typename T>
    typename std::enable_if<std::is_arithmetic<T>::value>::type
    print(T n, int base = 10)
    {
        if (base == HEX)
        {
            // Check if T is floating point
            if (std::is_floating_point<T>::value)
            {
                std::cout << n; // Hex doesn't apply to floats generally in Arduino print
            }
            else
            {
                std::cout << std::hex << (unsigned long long)n << std::dec;
            }
        }
        else
        {
            std::cout << n;
        }
    }

    // println overloads that just call print + newline
    void println(const char *s)
    {
        print(s);
        std::cout << std::endl;
    }

    void println(const String &s)
    {
        print(s);
        std::cout << std::endl;
    }

    void println(IPAddress ip)
    {
        print(ip);
        std::cout << std::endl;
    }

    void println()
    {
        std::cout << std::endl;
    }

    template <typename T>
    typename std::enable_if<std::is_arithmetic<T>::value>::type
    println(T n, int base = 10)
    {
        print(n, base);
        std::cout << std::endl;
    }
};
extern SerialMock Serial;

// Stream Mock
class Stream
{
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() = 0;
    virtual size_t write(uint8_t) = 0;
};

// HAL Mocks
typedef struct
{
    uint32_t Instance;
} RTC_HandleTypeDef;

#define RTC_BKP_DR0 0x00
#define RTC_BKP_DR1 0x01
#define RTC_BKP_DR2 0x02
#define RTC_BKP_DR3 0x03

inline uint32_t HAL_RTCEx_BKUPRead(RTC_HandleTypeDef *hrtc, uint32_t BackupRegister) { return 0; }
inline void HAL_RTCEx_BKUPWrite(RTC_HandleTypeDef *hrtc, uint32_t BackupRegister, uint32_t Data) {}

// External RTC handle declaration (defined in Arduino.cpp)
extern RTC_HandleTypeDef RTCHandle;

#endif
