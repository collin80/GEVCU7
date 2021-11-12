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

    //if (prefsHandler->checksumValid()) { //checksum is good, read in the values stored in EEPROM
        prefsHandler->read("Capacity", &config->packCapacity, 1000);
        prefsHandler->read("AHRemaining", (uint32_t *)&config->packAHRemaining, 5000000);
        prefsHandler->read("HVHighLim", &config->highVoltLimit, 3850);
        prefsHandler->read("HVLowLim", &config->lowVoltLimit, 2400);
        prefsHandler->read("CellHiLim", &config->highCellLimit, 3900);
        prefsHandler->read("CellLowLim", &config->lowCellLimit, 2400);
        prefsHandler->read("TempHighLim", &config->highTempLimit, 600);
        prefsHandler->read("TempLowLim", (uint16_t *)&config->lowTempLimit, -200);        
}

void BatteryManager::saveConfiguration() {
    BatteryManagerConfiguration *config = (BatteryManagerConfiguration *)getConfiguration();
    
    Device::saveConfiguration(); // call parent

    prefsHandler->write("AHRemaining", (uint32_t)config->packAHRemaining);
    prefsHandler->write("Capacity", config->packCapacity);
    prefsHandler->write("HVHighLim", config->highVoltLimit);
    prefsHandler->write("HVLowLim", config->lowVoltLimit);
    prefsHandler->write("CellHiLim", config->highCellLimit);
    prefsHandler->write("CellLowLim", config->lowCellLimit);
    prefsHandler->write("TempHighLim", config->highTempLimit);
    prefsHandler->write("TempLowLim", (uint16_t)config->lowTempLimit);    

    prefsHandler->saveChecksum();
}
