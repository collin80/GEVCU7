/*
 * Virtual time, GPIO no-ops, serial ports and the emulated I2C EEPROM that
 * sits behind Wire. The EEPROM follows the 24-series protocol MemCache uses:
 * device address 0x50 | bank, two address bytes, then data; a read is a
 * write of the two address bytes followed by requestFrom().
 */
#include <Arduino.h>
#include <TeensyThreads.h>
#include "host_test_support.h"

static uint64_t hostMicrosNow = 0;

uint32_t millis() { return (uint32_t)(hostMicrosNow / 1000); }
uint32_t micros() { return (uint32_t)hostMicrosNow; }
void delay(uint32_t ms) { hostMicrosNow += (uint64_t)ms * 1000; }
void delayMicroseconds(uint32_t us) { hostMicrosNow += us; }
void yield() {}
void hostSetMillis(uint32_t ms) { hostMicrosNow = (uint64_t)ms * 1000; }
void hostAdvanceMillis(uint32_t ms) { hostMicrosNow += (uint64_t)ms * 1000; }
void hostSetMicros(uint32_t us) { hostMicrosNow = us; }

void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t, uint8_t) {}
int digitalRead(uint8_t) { return 0; }
int analogRead(uint8_t) { return 0; }
void analogWrite(uint8_t, int) {}
void arm_dcache_flush(void *, uint32_t) {}

HardwareSerial Serial;
HardwareSerial Serial1;
HardwareSerial Serial2;
CrashReportClass CrashReport;

// ---------------------------------------------------------------- threads ---
Threads threads;
static int mutexMaxDepth = 0;

int Threads::Mutex::lock(unsigned int)
{
    depth++;
    if (depth > mutexMaxDepth) mutexMaxDepth = depth;
    return 1;
}
int Threads::Mutex::try_lock() { return lock(); }
int Threads::Mutex::unlock()
{
    if (depth > 0) depth--;
    return 1;
}
void Threads::delay(int ms) { ::delay((uint32_t)ms); }
void Threads::yield() {}
int hostMutexMaxDepth() { return mutexMaxDepth; }
void hostMutexReset() { mutexMaxDepth = 0; }

// ----------------------------------------------------------------- EEPROM ---
static uint8_t eeprom[HOST_EEPROM_SIZE];
static uint32_t pageWrites = 0;
static uint32_t pageReads = 0;
static bool readFailure = false;
static bool writeFailure = false;

static uint8_t txDevice = 0;
static uint8_t txBuf[2 + HOST_EEPROM_PAGE + 16];
static size_t txLen = 0;
static uint32_t currentAddr = 0; // address pointer latched by the last write
static std::vector<uint8_t> rxBuf;
static size_t rxPos = 0;

HostWire Wire;

void hostEepromErase() { memset(eeprom, 0xFF, sizeof(eeprom)); }
uint8_t *hostEepromData() { return eeprom; }
void hostEepromPoke(uint32_t addr, const void *data, size_t len) { memcpy(eeprom + addr, data, len); }
void hostEepromPeek(uint32_t addr, void *data, size_t len) { memcpy(data, eeprom + addr, len); }
uint32_t hostEepromPageWrites() { return pageWrites; }
uint32_t hostEepromPageReads() { return pageReads; }
void hostEepromResetCounters() { pageWrites = pageReads = 0; }
void hostEepromSetReadFailure(bool fail) { readFailure = fail; }
void hostEepromSetWriteFailure(bool fail) { writeFailure = fail; }

void HostWire::beginTransmission(uint8_t address)
{
    txDevice = address;
    txLen = 0;
}

size_t HostWire::write(uint8_t b)
{
    if (txLen < sizeof(txBuf)) txBuf[txLen++] = b;
    return 1;
}

size_t HostWire::write(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) write(buf[i]);
    return len;
}

uint8_t HostWire::endTransmission(bool)
{
    if ((txDevice & 0xFC) != 0x50) return 2; // address NACK - no such chip
    if (txLen < 2) return 4;
    uint32_t bank = (uint32_t)(txDevice & 0x03) << 16;
    currentAddr = bank | ((uint32_t)txBuf[0] << 8) | txBuf[1];
    if (txLen > 2)
    {
        if (writeFailure) return 3;
        // Page write: the low byte wraps inside the 256 byte page, as on the real chip.
        uint32_t pageBase = currentAddr & ~(HOST_EEPROM_PAGE - 1);
        uint32_t offset = currentAddr & (HOST_EEPROM_PAGE - 1);
        for (size_t i = 2; i < txLen; i++)
        {
            eeprom[pageBase + offset] = txBuf[i];
            offset = (offset + 1) & (HOST_EEPROM_PAGE - 1);
        }
        pageWrites++;
    }
    return 0;
}

uint8_t HostWire::requestFrom(uint8_t address, int quantity, bool)
{
    rxBuf.clear();
    rxPos = 0;
    if ((address & 0xFC) != 0x50 || readFailure) return 0;
    uint32_t addr = currentAddr;
    for (int i = 0; i < quantity; i++)
    {
        rxBuf.push_back(eeprom[addr % HOST_EEPROM_SIZE]);
        addr++;
    }
    pageReads++;
    return (uint8_t)quantity;
}

int HostWire::available() { return (int)(rxBuf.size() - rxPos); }
int HostWire::read() { return rxPos < rxBuf.size() ? rxBuf[rxPos++] : -1; }
