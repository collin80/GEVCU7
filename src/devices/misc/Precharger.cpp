/*
* Precharger.cpp - Implements various precharging strategies in case the user does not have
                   a bms or other device which is handling this.
 
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

#include "Precharger.h"

/*
 * Constructor
 */
Precharger::Precharger() : Device() {    
    commonName = "Precharge Controller";
    shortName = "Precharge";
    state = PRECHARGE_INIT;
    targetVoltage = 0;
    isPrecharged = false;
}

void Precharger::earlyInit()
{
    prefsHandler = new PrefHandler(PRECHARGER);
}

/*
 * Setup the device.
 */
void Precharger::setup() {
    crashHandler.addBreadcrumb(ENCODE_BREAD("PRECR") + 0);
    tickHandler.detach(this); // unregister from TickHandler first

    Logger::info("add device: Precharger (id: %X, %X)", PRECHARGER, this);

    loadConfiguration();

    Device::setup(); //call base class

    PrechargeConfiguration *config = (PrechargeConfiguration *)getConfiguration();

    cfgEntries.reserve(6);

    ConfigEntry entry;
    entry = {"PRECHARGETYPE", "Set precharge type (1 = Time Based, 2 = Voltage Based)", &config->prechargeType, CFG_ENTRY_VAR_TYPE::BYTE, 1, 2, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PRECHARGETIME", "Set precharge time in milliseconds", &config->prechargeTime, CFG_ENTRY_VAR_TYPE::UINT16, 0, 65000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PRECHARGERELAY", "Set output to use for precharge relay", &config->prechargeRelay, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"MAINCONTACTOR", "Set output to use for main contactor", &config->mainRelay, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PRECHARGE-TRIGGER", "Set input to use for triggering precharge", &config->enableInput, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);

    StatusEntry stat;
    //        name                       var         type               prevVal  obj
    stat = {"IsPrechargeComplete", &isPrecharged, CFG_ENTRY_VAR_TYPE::BYTE, 0, this};
    deviceManager.addStatusEntry(stat);

    tickHandler.attach(this, CFG_TICK_INTERVAL_PRECHARGE);
}

/*
old status bitfield stuff from motorcontroller class. Should find a way to integrate it
            statusBitfield2 &= ~(1 << 17); //clear bitTurn off MAIN CONTACTOR annunciator
            statusBitfield1 &= ~(1 << contactor);//clear bitTurn off main contactor output annunciator

            statusBitfield2 |=1 << 19; //set bit to turn on  PRECHARGE RELAY annunciator
            statusBitfield1 |=1 << relay; //set bit to turn ON precharge OUTPUT annunciator
*/


/*
 * Process a timer event. This is where you should be doing checks and updates. 
 */
void Precharger::handleTick() {
    crashHandler.addBreadcrumb(ENCODE_BREAD("PRECR") + 1);
    Device::handleTick(); // Call parent which controls the workflow
    //Logger::avalanche("Precharge Tick Handler");

    PrechargeConfiguration *config = (PrechargeConfiguration *) getConfiguration();

/*  
    there are two basic types of precharge - a raw time and thats it or we look at other devices to get voltages
    a BMS would know the pack voltage and something like a motor controller would know what voltage it currently sees.
    So, the idea is that we grab the actual pack voltage from the BMS then check the motor controller periodically
    to see if we've reached or nearly reached the voltage. If so we're good. If we exceed the precharge time set we'll
    fault because it obviously isn't working.
*/
    switch (state)
    {
    case PRECHARGE_INIT:
        //don't actually initialize until the enable pin goes high if we've got one.
        if (config->enableInput < 255)
        {
            if (!systemIO.getDigitalIn(config->enableInput)) return; //don't do any processing if we're not enabled
        }

        //need to set up to enter in progress
        prechargeBeginTime = millis();

        if (config->prechargeRelay != 255) 
        {
            Logger::info("Starting precharge by closing the precharge relay");
            systemIO.setDigitalOutput(config->prechargeRelay, true);            
        }
        if (config->prechargeType == PT_WAIT_FOR_VOLTAGE) 
        {
            BatteryManager *bms = static_cast<BatteryManager *>(deviceManager.getDeviceByType(DeviceType::DEVICE_BMS));
            if (bms) targetVoltage = bms->getPackVoltage();
        }
        state = PRECHARGE_INPROGRESS;
        isPrecharged = false;
        break;
    case PRECHARGE_INPROGRESS:
        //check on status, maybe fault, maybe set complete
        if (config->prechargeType == PT_TIME_DELAY)
        {
            if ( (millis() - prechargeBeginTime) >= config->prechargeTime) //done!
            {
                state = PRECHARGE_COMPLETE;
                if (config->mainRelay != 255) 
                {
                    systemIO.setDigitalOutput(config->mainRelay, true); //close main contactor
                    Logger::info("Precharge done. Closing main contactor.");
                }
            }
        }
        else if (config->prechargeType == PT_WAIT_FOR_VOLTAGE)
        {
            MotorController *mc = static_cast<MotorController *>(deviceManager.getDeviceByType(DeviceType::DEVICE_MOTORCTRL));
            if (mc)
            {
                float progress = (float)mc->getDcVoltage() / (float)targetVoltage;
                if (progress > 0.97f) 
                {
                    state = PRECHARGE_COMPLETE;
                    if (config->mainRelay != 255) 
                    {
                        systemIO.setDigitalOutput(config->mainRelay, true); //close main contactor
                        Logger::info("Precharge done. Closing main contactor.");
                    }
                }
            }
        }
        break;
    case PRECHARGE_COMPLETE:
        //nothing more to do!
        isPrecharged = true;
        break; 
    case PRECHARGE_FAULT: //if we fault then be sure to turn off the contactors
        if (config->prechargeRelay != 255) systemIO.setDigitalOutput(config->prechargeRelay, false);
        if (config->mainRelay != 255) systemIO.setDigitalOutput(config->mainRelay, false);
        Logger::error("Precharge faulted! Ensuring precharge relay and main contactor are open!");
        break;
    }
}

/*
 * Return the device ID
 */
DeviceId Precharger::getId() {
    return (PRECHARGER);
}

DeviceType Precharger::getType()
{
    return DEVICE_MISC;
}

/*
 * Load the device configuration.
 * If possible values are read from EEPROM. If not, reasonable default values
 * are chosen and the configuration is overwritten in the EEPROM.
 */
void Precharger::loadConfiguration() {
    PrechargeConfiguration *config = (PrechargeConfiguration *) getConfiguration();

    if (!config) { // as lowest sub-class make sure we have a config object
        config = new PrechargeConfiguration();
        Logger::debug("loading configuration in precharger");
        setConfiguration(config);
    }
    
    Device::loadConfiguration(); // call parent

    prefsHandler->read("PrechargeRelay", &config->prechargeRelay, 0);
    prefsHandler->read("PrechargeTime", &config->prechargeTime, 6000);
    prefsHandler->read("PrechargeType", &config->prechargeType, (uint8_t)PT_TIME_DELAY);
    prefsHandler->read("MainContactor", &config->mainRelay, 1);
    prefsHandler->read("PrechargeTrig", &config->enableInput, 255);
}

/*
 * Store the current configuration to EEPROM
 */
void Precharger::saveConfiguration() {
    PrechargeConfiguration *config = (PrechargeConfiguration *) getConfiguration();

    Device::saveConfiguration(); // call parent
    prefsHandler->write("PrechargeRelay", config->prechargeRelay);
    prefsHandler->write("PrechargeTime", config->prechargeTime);
    prefsHandler->write("PrechargeType", config->prechargeType);
    prefsHandler->write("MainContactor", config->mainRelay);
    prefsHandler->write("PrechargeTrig", config->enableInput);

    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}

Precharger precharge;
