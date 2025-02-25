/*
* SystemDevice.cpp - Storage and retrieval of system settings in the same
                     way as all the device drivers work
 
 Copyright (c) 2021 Collin Kidder

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

#include "SystemDevice.h"
#include "SD.h"

#define CFG_TICK_SYSTEM 40000

SystemConfiguration *sysConfig;
extern bool sdCardWorking;
extern uint8_t sdCardPresence;
extern bool sdCardInitFailed;

/*
 * Constructor
 */
SystemDevice::SystemDevice() : Device() {    
    commonName = "System";
    shortName = "SYS";
    deviceId = SYSTEM;
    deviceType = DEVICE_MISC;
    sysConfig = nullptr;
}

void SystemDevice::earlyInit()
{
    if (!prefsHandler) prefsHandler = new PrefHandler(SYSTEM);
}

/*
 * Setup the device.
 */
void SystemDevice::setup() {
    if (sysConfig != nullptr) return;
    Logger::info("add device: System (id: %X, %X)", SYSTEM, this);

    tickHandler.detach(this);//Turn off tickhandler

    loadConfiguration();
    
    Device::setup(); //call base class

    SystemConfiguration *config = (SystemConfiguration *)getConfiguration();

    cfgEntries.reserve(25);
    char buff[20];

    ConfigEntry entry;
    entry = {"SYSTYPE", "Set board revision level (0=7-A, 1=7-B, 2=7-C", &config->systemType  , CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"LOGLEVEL", "Set system logging level (0=Debug, 1=Info, 2=Warn, 3=Error 4=Off)", &config->logLevel, CFG_ENTRY_VAR_TYPE::INT16, {.s_int = -1}, 4, 0, nullptr};
    cfgEntries.push_back(entry);
    for (int i = 0; i < NUM_ANALOG; i++)
    {
        snprintf(buff, 20, "ADCGAIN%u", i);
        entry = {buff, "Set gain of ADC input. 1024 is 1 to 1 scaling", &config->adcGain[i], CFG_ENTRY_VAR_TYPE::UINT16, 0, 60000, 0, nullptr};
        cfgEntries.push_back(entry);
        snprintf(buff, 20, "ADCOFF%u", i);
        entry = {buff, "Set offset for ADC input. 0 is normal value", &config->adcOffset[i], CFG_ENTRY_VAR_TYPE::UINT16, 0, 60000, 0, nullptr};
        cfgEntries.push_back(entry);
    }

    entry = {"CAN0SPEED", "Set speed of CAN0 bus", &config->canSpeed[0], CFG_ENTRY_VAR_TYPE::UINT32, 33333, 1000000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"CAN1SPEED", "Set speed of CAN1 bus", &config->canSpeed[1], CFG_ENTRY_VAR_TYPE::UINT32, 33333, 1000000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"CAN2SPEED", "Set speed of CAN2 bus", &config->canSpeed[2], CFG_ENTRY_VAR_TYPE::UINT32, 33333, 1000000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"CANFDSPEED", "Set speed of FD mode data", &config->canSpeed[3], CFG_ENTRY_VAR_TYPE::UINT32, 500000, 8000000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"SWCANMODE", "Set whether CAN0 is in SingleWire mode (only with hardware mods)", &config->swcanMode, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr};
    cfgEntries.push_back(entry);

    tickHandler.attach(this, CFG_TICK_SYSTEM);
}

/*
 * Basically the only thing to do here is to figure out the state of the sdcard. There are a few states we care about:
 1. SDCard is not inserted and has not been inserted
 2. SDCard is not inserted but last we knew it was
 3. SDCard is inserted but last we knew it wasn't
 4. SDCard is inserted and has been
 States 1 and 4 are stable and we don't really care that much. The states to track and handle are really 2 and 3 
 */
void SystemDevice::handleTick()
{
    if (!digitalRead(SD_DETECT)) sdCardPresence++;
        else sdCardPresence = 0;
    if (sdCardPresence > 10) sdCardPresence = 10;
    if (!sdCardWorking && (sdCardPresence == 10) && !sdCardInitFailed) //freshly inserted
    {
        Logger::info("SDCard has been inserted. Attempting to initialize it.");
        if (!SD.sdfs.begin(SdioConfig(FIFO_SDIO)))
	    {
    	    //sdCard.initErrorHalt(&Serial);
            Logger::error("Could not initialize sdCard.");
            sdCardWorking = false;
            sdCardInitFailed = true;
  	    }
        else 
        {
            sdCardWorking = true;
            sdCardInitFailed = false;
            Logger::info("SDCard driver was initialized.");
            Logger::initializeFile();
        }
    }

    if (sdCardWorking && sdCardPresence == 0) //sdcard has been removed
    {
        Logger::info("SDCard has been removed! No logging, loading, or saving will be possible.");
        sdCardInitFailed = false;
        sdCardWorking = false;
    }
}

/*
 * Load the device configuration.
 * If possible values are read from EEPROM. If not, reasonable default values
 * are chosen and the configuration is overwritten in the EEPROM.
 */
void SystemDevice::loadConfiguration() {
    SystemConfiguration *config = (SystemConfiguration *) getConfiguration();

    if (!config) { // as lowest sub-class make sure we have a config object
        config = new SystemConfiguration();
        Logger::debug("loading configuration in system");
        setConfiguration(config);
    }

    sysConfig = config;
    
    Device::loadConfiguration(); // call parent

    uint8_t temp;
    prefsHandler->read("LogLevel", &temp, 1);
    config->logLevel = (int16_t) temp;
    if (config->logLevel == 255) config->logLevel = -1;
    prefsHandler->read("SysType", &config->systemType, 2); //revision level
    prefsHandler->read("Adc0Gain", &config->adcGain[0], 1024);
    prefsHandler->read("Adc0Offset", &config->adcOffset[0], 0);
    prefsHandler->read("Adc1Gain", &config->adcGain[1], 1024);
    prefsHandler->read("Adc1Offset", &config->adcOffset[1], 0);
    prefsHandler->read("Adc2Gain", &config->adcGain[2], 1024);
    prefsHandler->read("Adc2Offset", &config->adcOffset[2], 0);
    prefsHandler->read("Adc3Gain", &config->adcGain[3], 1024);
    prefsHandler->read("Adc3Offset", &config->adcOffset[3], 0);
    prefsHandler->read("Adc4Gain", &config->adcGain[4], 1024);
    prefsHandler->read("Adc4Offset", &config->adcOffset[4], 0);
    prefsHandler->read("Adc5Gain", &config->adcGain[5], 1024);
    prefsHandler->read("Adc5Offset", &config->adcOffset[5], 0);
    prefsHandler->read("Adc6Gain", &config->adcGain[6], 1024);
    prefsHandler->read("Adc6Offset", &config->adcOffset[6], 0);
    prefsHandler->read("Adc7Gain", &config->adcGain[7], 1024);
    prefsHandler->read("Adc7Offset", &config->adcOffset[7], 0);
    prefsHandler->read("CAN0Speed", &config->canSpeed[0], 500000);
    prefsHandler->read("CAN1Speed", &config->canSpeed[1], 500000);
    prefsHandler->read("CAN2Speed", &config->canSpeed[2], 500000);
    prefsHandler->read("CANFDSpeed", &config->canSpeed[3], 2000000);
    prefsHandler->read("SWCANMode", &config->swcanMode, 0);
}

/*
 * Store the current configuration to EEPROM
 */
void SystemDevice::saveConfiguration() {
    SystemConfiguration *config = (SystemConfiguration *) getConfiguration();

    Device::saveConfiguration(); // call parent

    prefsHandler->write("LogLevel", (uint8_t)config->logLevel);
    prefsHandler->write("SysType", config->systemType);
    prefsHandler->write("Adc0Gain", config->adcGain[0]);
    prefsHandler->write("Adc0Offset", config->adcOffset[0]);
    prefsHandler->write("Adc1Gain", config->adcGain[1]);
    prefsHandler->write("Adc1Offset", config->adcOffset[1]);
    prefsHandler->write("Adc2Gain", config->adcGain[2]);
    prefsHandler->write("Adc2Offset", config->adcOffset[2]);
    prefsHandler->write("Adc3Gain", config->adcGain[3]);
    prefsHandler->write("Adc3Offset", config->adcOffset[3]);
    prefsHandler->write("Adc4Gain", config->adcGain[4]);
    prefsHandler->write("Adc4Offset", config->adcOffset[4]);
    prefsHandler->write("Adc5Gain", config->adcGain[5]);
    prefsHandler->write("Adc5Offset", config->adcOffset[5]);
    prefsHandler->write("Adc6Gain", config->adcGain[6]);
    prefsHandler->write("Adc6Offset", config->adcOffset[6]);
    prefsHandler->write("Adc7Gain", config->adcGain[7]);
    prefsHandler->write("Adc7Offset", config->adcOffset[7]);
    prefsHandler->write("CAN0Speed", config->canSpeed[0]);
    prefsHandler->write("CAN1Speed", config->canSpeed[1]);
    prefsHandler->write("CAN2Speed", config->canSpeed[2]);
    prefsHandler->write("CANFDSpeed", config->canSpeed[3]);
    prefsHandler->write("SWCANMode", config->swcanMode);
    
    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}

DMAMEM SystemDevice sysDev;
