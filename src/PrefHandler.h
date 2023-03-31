/*
 * PrefHandler.h
 *
 * header for preference handler
 *
Copyright (c) 2013 Collin Kidder, Michael Neuweiler, Charles Galpin

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef PREF_HANDLER_H_
#define PREF_HANDLER_H_

#include <Arduino.h>
#include "config.h"
#include "eeprom_layout.h"
#include "MemCache.h"
#include "devices/DeviceTypes.h"
#include "Logger.h"

//normal or Last Known Good configuration
#define PREF_MODE_NORMAL  false
#define PREF_MODE_LKG     true

//we leave 20 bytes at the start of each device block. The first byte is the CRC, the next 2 are the device ID.
//After that are 17 reserved bytes.
#define SETTINGS_START  20

extern MemCache *memCache;

class PrefHandler {
public:

    PrefHandler();
    PrefHandler(DeviceId id);
    ~PrefHandler();
    void LKG_mode(bool mode);
    bool write(const char *key, uint8_t val);
    bool write(const char *key, uint16_t val);
    bool write(const char *key, uint32_t val);
    bool write(const char *key, float val);
    bool write(const char *key, double val);
    bool write(const char *key, const char *val, size_t maxlen);
    bool read(const char *key, uint8_t *val, uint8_t defval);
    bool read(const char *key, uint16_t *val, uint16_t defval);
    bool read(const char *key, uint32_t *val, uint32_t defval);
    bool read(const char *key, float *val, float defval);
    bool read(const char *key, double *val, double defval);
    bool read(const char *key, char *val, const char* defval);

    uint8_t calcChecksum();
    void saveChecksum();
    bool checksumValid();
    void forceCacheWrite();
    void resetEEPROM();
    bool isEnabled();
    void setEnabledStatus(bool en);
    static bool setDeviceStatus(uint16_t device, bool enabled);
    static void dumpDeviceTable();
    static void initDevTable();
    void checkTableValidity();

private:
    uint32_t base_address; //base address for the parent device
    uint32_t lkg_address;
    uint16_t deviceID; //device ID of the device that registered this pref handler instance
    bool use_lkg; //use last known good config?
    bool enabled;
    int position; //position within the device table
    volatile bool semKeyLookup;

    uint32_t fnvHash(const char *input);
    uint32_t findSettingLocation(uint32_t hash);
    uint32_t findEmptySettingLoc();
    uint32_t keyToAddress(const char *key, bool createIfNecessary);
    static void processAutoEntry(uint16_t val, uint16_t pos);
};

#endif



