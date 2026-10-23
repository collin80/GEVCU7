/*
 * Host-side (native) stand-in for the Teensy Arduino core.
 *
 * Only what the firmware modules under test actually touch is provided here.
 * Timing is fully virtual: millis()/micros() return a counter that tests drive
 * with hostSetMillis()/hostAdvanceMillis() from host_test_support.h, and delay()
 * simply advances that counter instead of sleeping.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <string>
#include <algorithm>

#ifndef ARDUINO
#define ARDUINO 10819
#endif

#define FLASHMEM
#define DMAMEM
#define EXTMEM
#define PROGMEM
#define F(x) (x)

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define DEC 10
#define HEX 16
#define BIN 2

typedef bool boolean;
typedef uint8_t byte;

uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void yield();

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int val);

void arm_dcache_flush(void *addr, uint32_t size);

// Same integer arithmetic as the Teensy core's map(), which matters for exact
// expected values in the throttle tests.
template <class A, class B, class C, class D, class E>
long map(A x, B in_min, C in_max, D out_min, E out_max)
{
    return ((long)x - (long)in_min) * ((long)out_max - (long)out_min) / ((long)in_max - (long)in_min) + (long)out_min;
}

template <class T, class L, class H>
auto constrain(T amt, L low, H high) -> decltype(amt < low ? low : (amt > high ? high : amt))
{
    return amt < low ? low : (amt > high ? high : amt);
}

using std::min;
using std::max;

class String
{
public:
    String() {}
    String(const char *s) : str(s ? s : "") {}
    String(const std::string &s) : str(s) {}
    String(char c) : str(1, c) {}
    String(int v, int base = DEC) : str(fmtInt((long long)v, base)) {}
    String(unsigned int v, int base = DEC) : str(fmtUInt(v, base)) {}
    String(long v, int base = DEC) : str(fmtInt(v, base)) {}
    String(unsigned long v, int base = DEC) : str(fmtUInt(v, base)) {}
    String(unsigned char v, int base = DEC) : str(fmtUInt(v, base)) {}
    String(short v, int base = DEC) : str(fmtInt(v, base)) {}
    String(unsigned short v, int base = DEC) : str(fmtUInt(v, base)) {}
    String(float v, int digits = 2) : str(fmtDouble(v, digits)) {}
    String(double v, int digits = 2) : str(fmtDouble(v, digits)) {}

    const char *c_str() const { return str.c_str(); }
    unsigned int length() const { return (unsigned int)str.length(); }
    bool equals(const String &o) const { return str == o.str; }
    bool operator==(const String &o) const { return str == o.str; }
    bool operator==(const char *o) const { return str == (o ? o : ""); }
    bool operator!=(const String &o) const { return str != o.str; }
    String &operator+=(const String &o) { str += o.str; return *this; }
    String &operator+=(const char *o) { str += o; return *this; }
    String &operator+=(char c) { str += c; return *this; }
    friend String operator+(const String &a, const String &b) { return String(a.str + b.str); }
    char operator[](unsigned int i) const { return str[i]; }
    long toInt() const { return atol(str.c_str()); }
    float toFloat() const { return (float)atof(str.c_str()); }
    void toUpperCase() { for (auto &c : str) c = (char)toupper(c); }
    void trim()
    {
        size_t b = str.find_first_not_of(" \t\r\n");
        size_t e = str.find_last_not_of(" \t\r\n");
        str = (b == std::string::npos) ? "" : str.substr(b, e - b + 1);
    }

private:
    std::string str;
    static std::string fmtInt(long long v, int base)
    {
        if (base == DEC) return std::to_string(v);
        return fmtUInt((unsigned long long)v, base);
    }
    static std::string fmtUInt(unsigned long long v, int base)
    {
        if (v == 0) return "0";
        std::string out;
        while (v) { int d = (int)(v % base); out.insert(out.begin(), (char)(d < 10 ? '0' + d : 'A' + d - 10)); v /= base; }
        return out;
    }
    static std::string fmtDouble(double v, int digits)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.*f", digits, v);
        return buf;
    }
};

class CrashReportClass;

class Print
{
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t b) { (void)b; return 1; }
    virtual size_t write(const uint8_t *buf, size_t len) { for (size_t i = 0; i < len; i++) write(buf[i]); return len; }
    size_t write(const char *s) { return write((const uint8_t *)s, strlen(s)); }
    size_t print(const char *s) { return write(s); }
    size_t print(const String &s) { return write(s.c_str()); }
    size_t print(long v, int base = DEC) { return print(String(v, base)); }
    size_t println() { return write("\n"); }
    size_t println(const char *s) { return print(s) + println(); }
    size_t println(const String &s) { return print(s) + println(); }
    size_t println(long v, int base = DEC) { return print(v, base) + println(); }
    size_t println(const CrashReportClass &) { return println(); }
    int printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)))
    {
        char buf[512];
        va_list args;
        va_start(args, fmt);
        int n = vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write(buf);
        return n;
    }
};

class Stream : public Print
{
public:
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int peek() { return -1; }
    virtual void flush() {}
};

// A serial port that discards output and has a scriptable receive buffer.
class HardwareSerial : public Stream
{
public:
    void begin(uint32_t) {}
    int available() override { return (int)(rx.size() - rxPos); }
    int read() override { return rxPos < rx.size() ? (uint8_t)rx[rxPos++] : -1; }
    int peek() override { return rxPos < rx.size() ? (uint8_t)rx[rxPos] : -1; }
    void hostFeed(const char *s) { rx += s; }
    void hostReset() { rx.clear(); rxPos = 0; }
    explicit operator bool() const { return true; }

private:
    std::string rx;
    size_t rxPos = 0;
};

extern HardwareSerial Serial;
extern HardwareSerial Serial1;
extern HardwareSerial Serial2;

/*
 * The firmware's own Wire implementation (i2c_driver_wire.h) is bound to the
 * i.MX RT1062 I2C peripheral. Claim its include guard so the real header is
 * skipped and provide a Wire object backed by an emulated 24-series EEPROM
 * (see host_test_support.h for the backing store).
 */
#define I2C_DRIVER_WIRE_H

class HostWire : public Stream
{
public:
    void begin() {}
    void setClock(uint32_t) {}
    void beginTransmission(uint8_t address);
    size_t write(uint8_t b) override;
    size_t write(const uint8_t *buf, size_t len) override;
    uint8_t endTransmission(bool sendStop = true);
    uint8_t requestFrom(uint8_t address, int quantity, bool sendStop = true);
    int available() override;
    int read() override;
};

extern HostWire Wire;
using TwoWire = HostWire;

// Teensy CrashReport stand-in; never reports a crash on the host.
class CrashReportClass : public Print
{
public:
    explicit operator bool() const { return false; }
};
extern CrashReportClass CrashReport;

struct crashreport_breadcrumbs_struct
{
    unsigned int bitmask;
    unsigned int value[6];
    unsigned int checksum;
};
