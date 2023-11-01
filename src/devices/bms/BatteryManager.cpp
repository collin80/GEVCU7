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

    //ConfigEntry entry;
    //entry = {"CAPACITY", "Capacity of battery pack in ampere-hours", &config->packCapacity, CFG_ENTRY_VAR_TYPE::FLOAT, {.floating = 0.0}, {.floating = 100000.0}, 2, nullptr};
    //cfgEntries.push_back(entry);

    StatusEntry stat;
    //        name       var           type                  prevVal  obj
    stat = {"BMS_PackV", &packVoltage, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"BMS_PackC", &packCurrent, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"BMS_SOC", &SOC, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"BMS_LowestCellV", &lowestCellV, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"BMS_HighestCellV", &highestCellV, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"BMS_LowestCellT", &lowestCellTemp, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"BMS_HighestCellT", &highestCellTemp, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"BMS_dischargeLimit", &dischargeLimit, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"BMS_chargeLimit", &chargeLimit, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);

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

float BatteryManager::getChargeLimit()
{
    return chargeLimit;
}

float BatteryManager::getDischargeLimit()
{
    return dischargeLimit;
}

void BatteryManager::loadConfiguration() {
    BatteryManagerConfiguration *config = (BatteryManagerConfiguration *)getConfiguration();

    Device::loadConfiguration(); // call parent
     
}

void BatteryManager::saveConfiguration() {
    BatteryManagerConfiguration *config = (BatteryManagerConfiguration *)getConfiguration();
    
    Device::saveConfiguration(); // call parent

    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}
