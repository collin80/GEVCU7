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
