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
    entry = {"CAPACITY", "Capacity of battery pack in ampere-hours", &config->packCapacity, CFG_ENTRY_VAR_TYPE::FLOAT, 0, 1000000, 2, nullptr};
    cfgEntries.push_back(entry);
    //entry = {"AHLEFT", "Number of amp hours remaining in pack in tenths ampere-hours", &config->packAHRemaining / 100000, CFG_ENTRY_VAR_TYPE::INT32, 0, 1000000, 0, nullptr};
    //cfgEntries.push_back(entry);
    entry = {"VOLTLIMHI", "High limit for pack voltage in volts", &config->highVoltLimit, CFG_ENTRY_VAR_TYPE::FLOAT, 0, 800, 1, nullptr};
    cfgEntries.push_back(entry);
    entry = {"VOLTLIMLO", "Low limit for pack voltage in volts", &config->lowVoltLimit, CFG_ENTRY_VAR_TYPE::FLOAT, 0, 800, 1, nullptr};
    cfgEntries.push_back(entry);
    entry = {"CELLLIMHI", "High limit for cell voltage in volts", &config->highCellLimit, CFG_ENTRY_VAR_TYPE::FLOAT, 0, 5.0f, 3, nullptr};
    cfgEntries.push_back(entry);
    entry = {"CELLLIMLO", "Low limit for cell voltage in hundredths of a volt", &config->lowCellLimit, CFG_ENTRY_VAR_TYPE::FLOAT, 0, 5.0f, 3, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TEMPLIMHI", "High limit for pack and cell temperature in degrees C", &config->highTempLimit, CFG_ENTRY_VAR_TYPE::FLOAT, 0, 255.0f, 1, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TEMPLIMLO", "Low limit for pack and cell temperature in degrees C", &config->lowTempLimit, CFG_ENTRY_VAR_TYPE::FLOAT, -255.0f, 255.0f, 1, nullptr};
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

float BatteryManager::getPackVoltage()
{
    return packVoltage;
}

float BatteryManager::getPackCurrent()
{
    return packCurrent;
}

float BatteryManager::getSOC()
{
    return SOC;
}

float BatteryManager::getHighestTemperature()
{
    return highestCellTemp;
}

float BatteryManager::getLowestTemperature()
{
    return lowestCellTemp;
}

void BatteryManager::loadConfiguration() {
    BatteryManagerConfiguration *config = (BatteryManagerConfiguration *)getConfiguration();

    Device::loadConfiguration(); // call parent

    //if (prefsHandler->checksumValid()) { //checksum is good, read in the values stored in EEPROM
        prefsHandler->read("Capacity", &config->packCapacity, 100.0f);
        prefsHandler->read("AHRemaining", &config->packAHRemaining, 50.0f);
        prefsHandler->read("HVHighLim", &config->highVoltLimit, 385.0f);
        prefsHandler->read("HVLowLim", &config->lowVoltLimit, 240.0f);
        prefsHandler->read("CellHiLim", &config->highCellLimit, 3.900f);
        prefsHandler->read("CellLowLim", &config->lowCellLimit, 2.400f);
        prefsHandler->read("TempHighLim", &config->highTempLimit, 60.0f);
        prefsHandler->read("TempLowLim", &config->lowTempLimit, -20.0f);        
}

void BatteryManager::saveConfiguration() {
    BatteryManagerConfiguration *config = (BatteryManagerConfiguration *)getConfiguration();
    
    Device::saveConfiguration(); // call parent

    prefsHandler->write("AHRemaining", config->packAHRemaining);
    prefsHandler->write("Capacity", config->packCapacity);
    prefsHandler->write("HVHighLim", config->highVoltLimit);
    prefsHandler->write("HVLowLim", config->lowVoltLimit);
    prefsHandler->write("CellHiLim", config->highCellLimit);
    prefsHandler->write("CellLowLim", config->lowCellLimit);
    prefsHandler->write("TempHighLim", config->highTempLimit);
    prefsHandler->write("TempLowLim", config->lowTempLimit);    

    prefsHandler->saveChecksum();
}
