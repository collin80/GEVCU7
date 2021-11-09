/*
 * BatteryManager.cpp
 *
 * Parent class for all battery management/monitoring systems
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

#include "BatteryManager.h"

BatteryManager::BatteryManager() : Device()
{
    packVoltage = 0;
    packCurrent = 0;
    SOC = 0;
}

BatteryManager::~BatteryManager()
{
}

DeviceType BatteryManager::getType() {
    return (DEVICE_BMS);
}

void BatteryManager::handleTick() {
    
}

void BatteryManager::setup() {

    BatteryManagerConfiguration *config = (BatteryManagerConfiguration *) getConfiguration();

    ConfigEntry entry;
    entry = {"CAPACITY", "Capacity of battery pack in tenths ampere-hours", &config->packCapacity, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000000};
    cfgEntries.push_back(entry);
    //entry = {"AHLEFT", "Number of amp hours remaining in pack in tenths ampere-hours", &config->packAHRemaining / 100000, CFG_ENTRY_VAR_TYPE::INT32, 0, 1000000};
    //cfgEntries.push_back(entry);
    entry = {"VOLTLIMHI", "High limit for pack voltage in tenths of a volt", &config->highVoltLimit, CFG_ENTRY_VAR_TYPE::UINT16, 0, 8000};
    cfgEntries.push_back(entry);
    entry = {"VOLTLIMLO", "Low limit for pack voltage in tenths of a volt", &config->lowVoltLimit, CFG_ENTRY_VAR_TYPE::UINT16, 0, 8000};
    cfgEntries.push_back(entry);
    entry = {"CELLLIMHI", "High limit for cell voltage in hundredths of a volt", &config->highCellLimit, CFG_ENTRY_VAR_TYPE::UINT16, 0, 500};
    cfgEntries.push_back(entry);
    entry = {"CELLLIMLO", "Low limit for cell voltage in hundredths of a volt", &config->lowCellLimit, CFG_ENTRY_VAR_TYPE::UINT16, 0, 500};
    cfgEntries.push_back(entry);
    entry = {"TEMPLIMHI", "High limit for pack and cell temperature in tenths of a degree C", &config->highTempLimit, CFG_ENTRY_VAR_TYPE::UINT16, 0, 2550};
    cfgEntries.push_back(entry);
    entry = {"TEMPLIMLO", "Low limit for pack and cell temperature in tenths of a degree C", &config->lowTempLimit, CFG_ENTRY_VAR_TYPE::INT16, -2550, 2550};
    cfgEntries.push_back(entry);

#ifndef USE_HARD_CODED
    if (prefsHandler->checksumValid()) { //checksum is good, read in the values stored in EEPROM
    }
    else { //checksum invalid. Reinitialize values and store to EEPROM
        //prefsHandler->saveChecksum();
    }
#else
#endif

//TickHandler::getInstance()->detach(this);
//TickHandler::getInstance()->attach(this, CFG_TICK_INTERVAL_MOTOR_CONTROLLER_DMOC);

}

int BatteryManager::getPackVoltage()
{
    return packVoltage;
}

signed int BatteryManager::getPackCurrent()
{
    return packCurrent;
}

int BatteryManager::getSOC()
{
    return SOC;
}

void BatteryManager::loadConfiguration() {
    BatteryManagerConfiguration *config = (BatteryManagerConfiguration *)getConfiguration();

    Device::loadConfiguration(); // call parent

#ifdef USE_HARD_CODED
    if (false) {
#else
    if (prefsHandler->checksumValid()) { //checksum is good, read in the values stored in EEPROM
#endif
        prefsHandler->read(EEBMS_CAPACITY, &config->packCapacity);
        prefsHandler->read(EEBMS_AH, (uint32_t *)&config->packAHRemaining);
        prefsHandler->read(EEBMS_HI_VOLT_LIM, &config->highVoltLimit);
        prefsHandler->read(EEBMS_LO_VOLT_LIM, &config->lowVoltLimit);
        prefsHandler->read(EEBMS_HI_CELL_LIM, &config->highCellLimit);
        prefsHandler->read(EEBMS_LO_CELL_LIM, &config->lowCellLimit);
        prefsHandler->read(EEBMS_HI_TEMP_LIM, &config->highTempLimit);
        prefsHandler->read(EEBMS_LO_TEMP_LIM, (uint16_t *)&config->lowTempLimit);        
    }
    else { //checksum invalid. Reinitialize values and store to EEPROM
        config->packCapacity = DefaultPackCapacity;
        config->packAHRemaining = DefaultPackRemaining;
        config->highVoltLimit = DefaultHighVLim;
        config->lowVoltLimit = DefaultLowVLim;
        config->highCellLimit = DefaultHighCellLim;
        config->lowCellLimit = DefaultLowCellLim;
        config->highTempLimit = DefaultHighTempLim;
        config->lowTempLimit = DefaultLowTempLim;
        saveConfiguration();
    }
}

void BatteryManager::saveConfiguration() {
    BatteryManagerConfiguration *config = (BatteryManagerConfiguration *)getConfiguration();
    
    Device::saveConfiguration(); // call parent

    prefsHandler->write(EEBMS_AH, (uint32_t)config->packAHRemaining);
    prefsHandler->write(EEBMS_CAPACITY, config->packCapacity);
    prefsHandler->write(EEBMS_HI_VOLT_LIM, config->highVoltLimit);
    prefsHandler->write(EEBMS_LO_VOLT_LIM, config->lowVoltLimit);
    prefsHandler->write(EEBMS_HI_CELL_LIM, config->highCellLimit);
    prefsHandler->write(EEBMS_LO_CELL_LIM, config->lowCellLimit);
    prefsHandler->write(EEBMS_HI_TEMP_LIM, config->highTempLimit);
    prefsHandler->write(EEBMS_LO_TEMP_LIM, (uint16_t)config->lowTempLimit);    

    prefsHandler->saveChecksum();
}
