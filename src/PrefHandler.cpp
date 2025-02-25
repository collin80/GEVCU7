/*
 * PrefHandler.cpp
 *
 * Abstracts away the particulars of how preferences are stored.
 * Transparently supports main and "last known good" storage and retrieval
 *
Copyright (c) 2013-2025 Collin Kidder, Michael Neuweiler, Charles Galpin

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

#include "PrefHandler.h"

PrefHandler::PrefHandler() {
    lkg_address = EE_MAIN_OFFSET; //default to normal mode
    base_address = 0;
    semKeyLookup = false;
}

bool PrefHandler::isEnabled()
{
    return enabled;
}

void PrefHandler::setEnabledStatus(bool en)
{
    uint16_t id = this->deviceID;

    enabled = en;

    //This should not be necessary. It's being done elsewhere.
    //if (enabled) {
    //    id |= 0x8000; //set enabled bit
    //}
    //else {
    //    id &= 0x7FFF; //clear enabled bit
    //}

    //memCache->Write(EE_DEVICE_TABLE + (2 * position), id);
}

void PrefHandler::dumpDeviceTable()
{
    uint16_t id;
    
    for (int x = 0; x < CFG_DEV_MGR_MAX_DEVICES; x++) 
    {
        memCache->Read(EE_DEVICE_TABLE + (2 * x), &id);
        Logger::console("Device ID: %X, Enabled = %X", id & 0x7FFF, id & 0x8000);
    }
}

void PrefHandler::checkTableValidity()
{
    uint16_t id;
    uint8_t failures = 0;

    while (failures < 3)
    {
        memCache->Read(EE_DEVICE_TABLE, &id);
        if (id == 0xDEAD) return;
        failures++;
        delay(5); //just a small delay
        memCache->InvalidateAll(); //clear the cache so the next read is from EEPROM not the cache
    }

    //if there were three failures in a row then we have to assume that the device table really is gone
    initDevTable();
}

void PrefHandler::processAutoEntry(uint16_t val, uint16_t pos)
{
    if (val < 0x7FFF) val = val | 0x8000; //automatically set enabled bit
        else val = 0;

    memCache->Write(EE_DEVICE_TABLE + (2 * pos), val);

    if (val == 0) return;
    val &= 0x7FFF;
    //immediately store our ID into the proper place
    memCache->Write(EE_DEVICE_ID + EE_DEVICES_BASE + (EE_DEVICE_SIZE * pos), val);
    Logger::info("Device ID: %X was placed into device table at entry: %i", (int)val, pos);      
}

void PrefHandler::initDevTable()
{
    uint16_t id;
    
    Logger::console("Initializing EEPROM device table");
    
    //First six are done from entries in config.h to automatically enable those devices
    processAutoEntry(AUTO_ENABLE_DEV1, 1);
    processAutoEntry(AUTO_ENABLE_DEV2, 2);
    processAutoEntry(AUTO_ENABLE_DEV3, 3);
    processAutoEntry(AUTO_ENABLE_DEV4, 4);
    processAutoEntry(AUTO_ENABLE_DEV5, 5);
    processAutoEntry(AUTO_ENABLE_DEV6, 6);
        
    //initialize table with zeros
    id = 0;
    for (int x = 7; x < CFG_DEV_MGR_MAX_DEVICES; x++) {
        memCache->Write(EE_DEVICE_TABLE + (2 * x), id);
    }

    //write out magic entry
    id = 0xDEAD;
    memCache->Write(EE_DEVICE_TABLE, id);
    memCache->FlushAllPages();
}

//Given a device ID we must search the 64 entry table found in EEPROM to see if the device
//has a spot in EEPROM. If it does not then add it
PrefHandler::PrefHandler(DeviceId id_in) {
    uint16_t id;

    enabled = false;
    semKeyLookup = false;

    checkTableValidity();

    for (int x = 1; x < CFG_DEV_MGR_MAX_DEVICES; x++) {
        memCache->Read(EE_DEVICE_TABLE + (2 * x), &id);
        if ((id & 0x7FFF) == ((int)id_in)) {
            base_address = EE_DEVICES_BASE + (EE_DEVICE_SIZE * x);
            lkg_address = EE_MAIN_OFFSET;
            if (id & 0x8000) enabled = true;
            position = x;
            deviceID = (uint16_t)id_in;
            Logger::debug("Device ID: %X was found in device table at entry: %i", (int)id_in, x);
            return;
        }
    }

    //if we got here then there was no entry for this device in the table yet.
    //try to find an empty spot and place it there.
    for (int x = 1; x < CFG_DEV_MGR_MAX_DEVICES; x++) {
        memCache->Read(EE_DEVICE_TABLE + (2 * x), &id);
        if ((id & 0x7FFF) == 0) {
            base_address = EE_DEVICES_BASE + (EE_DEVICE_SIZE * x);
            lkg_address = EE_MAIN_OFFSET;
            enabled = false; //default to devices being off until the user says otherwise
            id = (int16_t)id_in;
            memCache->Write(EE_DEVICE_TABLE + (2*x), id);
            position = x;
            deviceID = (uint16_t)id_in;
            //immediately store our ID into the proper place
            memCache->Write(EE_DEVICE_ID + base_address + lkg_address, deviceID);
            Logger::info("Device ID: %X was placed into device table at entry: %i", (int)id, x);
            memCache->FlushAllPages();
            return;
        }
    }

    //we found no matches and could not allocate a space. This is bad. Error out here
    base_address = 0xF0F0;
    lkg_address = EE_MAIN_OFFSET;
    Logger::error("PrefManager - Device Table Full!!!");
}

//A special static function that can be called whenever, wherever to turn a specific device on/off. Does not
//attempt to do so at runtime so the user will still have to power cycle to change the device status.
//returns true if it could make the change, false if it could not.
bool PrefHandler::setDeviceStatus(uint16_t device, bool enabled)
{
    uint16_t id;

    for (int x = 1; x < CFG_DEV_MGR_MAX_DEVICES; x++) {
        memCache->Read(EE_DEVICE_TABLE + (2 * x), &id);
        if ((id & 0x7FFF) == (device & 0x7FFF)) {
            Logger::avalanche("Found a device record to edit");
            if (enabled)
            {
                device |= 0x8000;
            }
            Logger::avalanche("ID to write: %X", device);
            memCache->Write(EE_DEVICE_TABLE + (2 * x), device);
            return true;
        }
    }
    return false;
}

PrefHandler::~PrefHandler() {
}

void PrefHandler::LKG_mode(bool mode) {
    if (mode) lkg_address = EE_LKG_OFFSET;
    else lkg_address = EE_MAIN_OFFSET;
}

/*
The whole system works on hashes now. All settings are stored as a hash of the setting name plus
the length, followed by the actual data bytes. This allows for storing anything you want in preferences
with nice descriptive names. Below are a series of functions that transparently implement this for the 
actual drivers
*/

//given a hash value it looks for that in the table. If it finds
//the hash it'll return 5 bytes higher which skips the hash and length
//so the return location will be the start of the actual value itself.
uint32_t PrefHandler::findSettingLocation(uint32_t hash)
{
    while (semKeyLookup);
    semKeyLookup = true;
    Logger::avalanche("Key lookup for %x", hash);
    uint32_t readHash;
    uint8_t readLength;
    for (uint32_t idx = SETTINGS_START; idx < EE_DEVICE_SIZE;)
    {
        memCache->Read((uint32_t)idx + base_address + lkg_address, &readHash);
        if (readHash == hash) //matched! return the address + 5 which is the start of the actual value
        {
            semKeyLookup = false;
            return (idx + 5);
        }
        //otherwise, read length then skip the 4 for hash, 1 for length, and length too.
        idx += 4;
        memCache->Read((uint32_t)idx + base_address + lkg_address, &readLength);
        idx += 1 + readLength;
    }
    semKeyLookup = false;
    return 0xFFFFFFFFul;
}

//Works similarly to above but finds a hash value that has not been initialized and
//returns the address of the hash value (not 5 higher as with the above function)
uint32_t PrefHandler::findEmptySettingLoc()
{
    uint32_t readHash;
    uint8_t readLength;
    for (uint32_t idx = SETTINGS_START; idx < EE_DEVICE_SIZE;)
    {
        memCache->Read((uint32_t)idx + base_address + lkg_address, &readHash);
        Logger::avalanche("Read Hash = %x", readHash);
        if (readHash == 0xFFFFFFFFul) //it's empty! Return this exact address
        {
            return (idx);
        }
        //otherwise, read length then skip the 4 for hash, 1 for length, and length too.
        idx += 4;
        memCache->Read((uint32_t)idx + base_address + lkg_address, &readLength);
        Logger::avalanche("Read length: %u", readLength);
        idx += 1 + readLength;
    }
    return 0xFFFFFFFFul;
}

//Uses the above two funcctions to input a key name and see where it is stored
//(if it exists). Can create a new entry if desired. Returns the location of 
//the setting. Not to be called by external code.
uint32_t PrefHandler::keyToAddress(const char *key, bool createIfNecessary)
{
    Logger::avalanche("Key look up for %s", key);
    uint32_t hash = fnvHash(key);
    uint32_t address = findSettingLocation(hash);
    if (address >= EE_DEVICE_SIZE) 
    {
        if (createIfNecessary)
        {
            Logger::avalanche("Must create new entry for this setting");
            address = findEmptySettingLoc();        
            if (address >= EE_DEVICE_SIZE) return 0xFFFFFFFFul;
            //write the hash value to this entry because it's new
            Logger::avalanche("Setting stored at %x", address);
            memCache->Write((uint32_t)address + base_address + lkg_address, hash);
            uint8_t len = 0; //don't know length yet. Set it to zero.
            address += 4; //increment past hash location
            memCache->Write((uint32_t)address + base_address + lkg_address, len);
            address += 1; //increment past length too
        }
    }
    Logger::avalanche("Key: %s Returned Addr: %x", key, address);
    return address;
}

//Now we have the actual functions that drivers will call to read and write things

//Given a key, write an 8 bit value with that key name
bool PrefHandler::write(const char *key, uint8_t val) {
    uint32_t address = keyToAddress(key, true);
    uint8_t len;
    memCache->Read((uint32_t)address + base_address + lkg_address - 1, &len);
    if (len == 0)
    {
        len = 1;
        memCache->Write((uint32_t)address + base_address + lkg_address - 1, len);
    }
    else if (len != 1)
    {
        Logger::error("Attempt to write improper length to variable %s!", key);
        return false;
    }
    //then return whether we could write the value into the memory cache
    return memCache->Write((uint32_t)address + base_address + lkg_address, val);
}

bool PrefHandler::write(const char *key, uint16_t val) {
    uint32_t address = keyToAddress(key, true);
    uint8_t len;
    memCache->Read((uint32_t)address + base_address + lkg_address - 1, &len);
    if (len == 0)
    {
        len = 2;
        memCache->Write((uint32_t)address + base_address + lkg_address - 1, len);
    }
    else if (len != 2)
    {
        Logger::error("Attempt to write improper length to variable %s!", key);
        return false;
    }
    //then return whether we could write the value into the memory cache    
    return memCache->Write((uint32_t)address + base_address + lkg_address, val);
}

bool PrefHandler::write(const char *key, uint32_t val) {
    uint32_t address = keyToAddress(key, true);
    uint8_t len;
    memCache->Read((uint32_t)address + base_address + lkg_address - 1, &len);
    if (len == 0)
    {
        len = 4;
        memCache->Write((uint32_t)address + base_address + lkg_address - 1, len);
    }
    else if (len != 4)
    {
        Logger::error("Attempt to write improper length to variable %s!", key);
        return false;
    }
    //then return whether we could write the value into the memory cache    
    return memCache->Write((uint32_t)address + base_address + lkg_address, val);
}

bool PrefHandler::write(const char *key, float val) {
    uint32_t address = keyToAddress(key, true);    
    uint8_t len;
    memCache->Read((uint32_t)address + base_address + lkg_address - 1, &len);
    if (len == 0)
    {
        len = 4;
        memCache->Write((uint32_t)address + base_address + lkg_address - 1, len);
    }
    else if (len != 4)
    {
        Logger::error("Attempt to write improper length to variable %s!", key);
        return false;
    }
    //then return whether we could write the value into the memory cache    
    return memCache->Write((uint32_t)address + base_address + lkg_address, val);
}

bool PrefHandler::write(const char *key, double val) {
    uint32_t address = keyToAddress(key, true);
    uint8_t len;
    memCache->Read((uint32_t)address + base_address + lkg_address - 1, &len);
    if (len == 0)
    {
        len = 8;
        memCache->Write((uint32_t)address + base_address + lkg_address - 1, len);
    }
    else if (len != 8)
    {
        Logger::error("Attempt to write improper length to variable %s!", key);
        return false;
    }
    //then return whether we could write the value into the memory cache    
    return memCache->Write((uint32_t)address + base_address + lkg_address, val);
}

bool PrefHandler::write(const char *key, const char *val, size_t maxlen) {
    uint32_t address = keyToAddress(key, true);    
    uint8_t len;
    size_t stringLen = strlen(val);
    if (stringLen > maxlen) stringLen = maxlen;
    memCache->Read((uint32_t)address + base_address + lkg_address - 1, &len);
    if (len == 0)
    {
        len = maxlen + 1;
        memCache->Write((uint32_t)address + base_address + lkg_address - 1, len);
    }
    else if (len != (maxlen + 1))
    {
        Logger::error("Attempt to write improper length to variable %s!", key);
        return false;
    }
    //then return whether we could write the value into the memory cache    
    return memCache->Write((uint32_t)address + base_address + lkg_address, val, stringLen + 1);
}

bool PrefHandler::writeBlock(const char *key, uint8_t *data, size_t length)
{
    uint32_t address = keyToAddress(key, true);    
    uint8_t len;
    
    memCache->Read((uint32_t)address + base_address + lkg_address - 1, &len);
    if (len == 0)
    {
        len = length;
        memCache->Write((uint32_t)address + base_address + lkg_address - 1, len);
    }
    else if (len != length)
    {
        Logger::error("Attempt to write improper length to variable %s!", key);
        return false;
    }
    //then return whether we could write the value into the memory cache    
    return memCache->Write((uint32_t)address + base_address + lkg_address, data, length);
}

bool PrefHandler::read(const char *key, uint8_t *val, uint8_t defval) {
    uint32_t address = keyToAddress(key, false);
    if (address < EE_DEVICE_SIZE) return memCache->Read((uint32_t)address + base_address + lkg_address, val);
    else 
    {
        *val = defval;
        return true;
    }
}

bool PrefHandler::read(const char *key, uint16_t *val, uint16_t defval) {
    uint32_t address = keyToAddress(key, false);
    if (address < EE_DEVICE_SIZE) return memCache->Read((uint32_t)address + base_address + lkg_address, val);
    else 
    {
        *val = defval;
        return true;
    }
}

bool PrefHandler::read(const char *key, uint32_t *val, uint32_t defval) {
    uint32_t address = keyToAddress(key, false);
    if (address < EE_DEVICE_SIZE) return memCache->Read((uint32_t)address + base_address + lkg_address, val);
    else 
    {
        *val = defval;
        return true;
    }
}

bool PrefHandler::read(const char *key, float *val, float defval) {
    uint32_t address = keyToAddress(key, false);
    if (address < EE_DEVICE_SIZE) return memCache->Read((uint32_t)address + base_address + lkg_address, val);
    else 
    {
        *val = defval;
        return true;
    }
}

bool PrefHandler::read(const char *key, double *val, double defval) {
    uint32_t address = keyToAddress(key, false);
    if (address < EE_DEVICE_SIZE) return memCache->Read((uint32_t)address + base_address + lkg_address, val);
    else 
    {
        *val = defval;
        return true;
    }
}

bool PrefHandler::read(const char *key, char *val, const char* defval)
{
    uint32_t address = keyToAddress(key, false);
    if (address < EE_DEVICE_SIZE) 
    {
        uint8_t c;
        int i = 0;
        while ( memCache->Read((uint32_t)address + base_address + lkg_address + i, &c) )
        {
            *val++ = c;
            i++;
            if (c == 0) break;
        }
        *val = 0;
        return true;
    }
    else 
    {
        strcpy(val, defval);
        return true;
    }
}

bool PrefHandler::readBlock(const char *key, uint8_t *data, size_t length) {
    uint32_t address = keyToAddress(key, false);
    if (address < EE_DEVICE_SIZE) return memCache->Read((uint32_t)address + base_address + lkg_address, data, length);
    else 
    {
        return false; //was not found. Return false to let caller know
    }
}

uint8_t PrefHandler::calcChecksum() {
    uint16_t counter;
    uint8_t accum = 0;
    uint8_t temp;
    for (counter = 1; counter < EE_DEVICE_SIZE; counter++) {
        memCache->Read((uint32_t)counter + base_address + lkg_address, &temp);
        accum += temp;
    }
    return accum;
}

//calculate the current checksum and save it to the proper place
void PrefHandler::saveChecksum() {
    uint8_t csum;
    csum = calcChecksum();
    Logger::debug("New checksum: %x", csum);
    memCache->Write(EE_CHECKSUM + base_address + lkg_address, csum);
}

bool PrefHandler::checksumValid() {
    //get checksum from EEPROM and calculate the current checksum to see if they match
    uint8_t stored_chk, calc_chk;
    uint16_t stored_id;

    memCache->Read(EE_CHECKSUM + base_address + lkg_address, &stored_chk);
    
    calc_chk = calcChecksum();
    
    if (calc_chk != stored_chk)
    {
        Logger::error("Checksum didn't match        Stored: %X Calc: %X", stored_chk, calc_chk);
        return false;
    }    

    Logger::info("Checksum matches Value: %X", calc_chk);
    
    memCache->Read(EE_DEVICE_ID + base_address + lkg_address, &stored_id);
    if (stored_id == 0xFFFF) //didn't used to store the device ID properly so fix that
    {
        stored_id = deviceID;
        memCache->Write(EE_DEVICE_ID + base_address + lkg_address, deviceID);
    }
    if (stored_id != deviceID)
    {
        Logger::error("ID mismatch in EEPROM. Resetting settings.        Stored: %X Proper: %X", stored_id, deviceID);
        return false;
    }

    return true;
}

void PrefHandler::forceCacheWrite()
{
    memCache->FlushAllPages();
}

//Use FNV-1a hash to turn an input string into a 32 bit hash value.
uint32_t PrefHandler::fnvHash(const char *input)
{
    uint32_t hash = 2166136261ul;
    char c;
    while (*input)
    {
        c = *input++;
        c = toupper(c); //force all names to uppercase just to make it consistent
        hash = hash ^ c;
        hash = hash * 16777619;
    }
    return hash;
}

//Resets the EEPROM storage for this particular PrefHandler instance. Everything will be reset
//to 0xFF's and the cache flushed so the settings will be fresh thereafter. 
void PrefHandler::resetEEPROM()
{
    //write over all the settings 32 bits at a time
    uint32_t val = 0xFFFFFFFFul;
    for (uint32_t idx = SETTINGS_START; idx < EE_DEVICE_SIZE; idx = idx + 4)
    {
        memCache->Write((uint32_t)idx + base_address + lkg_address, val);
    }
    memCache->FlushAllPages();
}
