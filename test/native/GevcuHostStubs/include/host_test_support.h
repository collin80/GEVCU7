/*
 * Controls and observation points for the host test doubles.
 *
 * Every test file should call hostResetAll() from setUp() so each test starts
 * with blank (0xFF) EEPROM, time at zero and empty logs.
 */
#pragma once

#include <Arduino.h>
#include <vector>
#include <string>

// ---- virtual time ----------------------------------------------------------
void hostSetMillis(uint32_t ms);
void hostAdvanceMillis(uint32_t ms);
void hostSetMicros(uint32_t us);

// ---- emulated 256KB I2C EEPROM (four 64KB banks at 0x50..0x53) -------------
#define HOST_EEPROM_SIZE (256u * 1024u)
#define HOST_EEPROM_PAGE 256u

void hostEepromErase();                          // fill with 0xFF
uint8_t *hostEepromData();                       // raw backing store
void hostEepromPoke(uint32_t addr, const void *data, size_t len);
void hostEepromPeek(uint32_t addr, void *data, size_t len);
uint32_t hostEepromPageWrites();                 // completed write transactions
uint32_t hostEepromPageReads();                  // completed read transactions
void hostEepromResetCounters();
void hostEepromSetReadFailure(bool fail);        // requestFrom() returns no data
void hostEepromSetWriteFailure(bool fail);       // endTransmission() NACKs, nothing stored

// ---- logger capture --------------------------------------------------------
struct HostLogRecord
{
    int level;          // 0=avalanche 1=debug 2=info 3=warn 4=error 5=console
    uint16_t device;    // 0xFFFF when no device given
    std::string text;
};
const std::vector<HostLogRecord> &hostLogRecords();
int hostLogCount(int level);
bool hostLogContains(int level, const char *substring);
void hostLogClear();

// ---- tick handler observation ---------------------------------------------
class TickObserver;
struct HostTickAttachment
{
    TickObserver *observer;
    uint32_t interval;
    std::string name;
};
const std::vector<HostTickAttachment> &hostTickAttachments();
void hostTickClear();

// ---- device manager observation -------------------------------------------
class Device;
struct StatusEntry;
const std::vector<Device *> &hostRegisteredDevices();
const std::vector<StatusEntry> &hostStatusEntries();
struct HostSentMessage
{
    int deviceType;
    uint16_t deviceId;
    uint32_t msgType;
};
const std::vector<HostSentMessage> &hostSentMessages();
void hostDeviceManagerClear();

// ---- threads ----------------------------------------------------------------
int hostMutexMaxDepth();   // deepest nesting seen on any Threads::Mutex
void hostMutexReset();

void hostResetAll();
