/*
* PowerController.cpp - Control state of power usage of GEVCU7 and potentially the rest of the system too
 
 Copyright (c) 2025 Collin Kidder

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

#include "PowerController.h"

/*
 * Constructor
 */
PowerController::PowerController() : Device() {    
    commonName = "Power Controller";
    shortName = "PwrCtrl";
    deviceType = DEVICE_MISC;
    deviceId = POWERCTRL;
    systemAwake = true;
    countdown = 0;
}

/*
 * Setup the device.
 */
void PowerController::setup() {
    crashHandler.addBreadcrumb(ENCODE_BREAD("POWRC") + 0);
    tickHandler.detach(this); // unregister from TickHandler first

    Logger::info("add device: POWER Controller (id: %X, %X)", POWERCTRL, this);

    loadConfiguration();

    Device::setup(); //call base class

    if (config->canbusNum != 255)
    {
        setAttachedCANBus(config->canbusNum);
        // register ourselves as observer of proper CAN message
        attachedCANBus->attach(this, config->powerTriggerCANID, 0x7ff, false);
    }

    cfgEntries.reserve(5);

    ConfigEntry entry;
    entry = {"PWRTRIGPIN", "Set pin to use for wake signal (255 = disabled)", &config->powerTriggerPin, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PWRTRIGID", "Set CAN ID for message that wakes the system", &config->powerTriggerCANID, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PWRTRIGBIT","Set bit of CAN message to use for waking up (255 = Disabled)", &config->powerTriggerCANBit, CFG_ENTRY_VAR_TYPE::INT16, {.s_int = -1000}, 0, 1, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PWRTRIGBUS","Set which CAN bus to listen on (0-2) or 255 to disable CAN listening", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PWROUTPIN","Set pin to activate when awake (255 = Disabled)", &config->powerOutputPin, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);

    tickHandler.attach(this, CFG_TICK_INTERVAL_POWER);
}

/*
 * Process a timer event. This is where you should be doing checks and updates. 
 */
void PowerController::handleTick() {
    //crashHandler.addBreadcrumb(ENCODE_BREAD("POWRC") + 1);
    Device::handleTick(); // Call parent which controls the workflow
    //Logger::avalanche("Power Tick Handler");
    if (config->powerTriggerPin != 255)
    {
        if (systemIO.getDigitalIn(config->powerTriggerPin))
        {
            wakeup();
        }
        else
        {
            snooze();
        }
    }
    if (countdown > 0)
    {
        countdown--;
        if ((countdown == 0) && (config->powerOutputPin != 255) )
        {
            memCache->FlushAllPages(); //write out everything pending. This routine blocks until it happens
            systemIO.setDigitalOutput(config->powerOutputPin, false); //and turn off power to everything if pin is set
        }
    }
    if (config->canbusNum < 3)
    {
        if (canTimer == 1) snooze();
        if (canTimer > 0) canTimer--;
    }
}

void PowerController::handleCanFrame(const CAN_message_t &frame)
{
    int byt = config->powerTriggerCANBit / 8;
    int bit = config->powerTriggerCANBit & 7;

    if (frame.id == config->powerTriggerCANID)
    {
        if ( (config->powerTriggerCANBit == 255) || (frame.buf[byt] & (1 << bit)) )
        {
            wakeup();
            canTimer = 20;
        }
    }
}

void PowerController::wakeup()
{
    if (!systemAwake)
    {
        //freshly awake after sleeping
        deviceManager.sendMessage(DeviceType::DEVICE_ANY, ANYDEVICE, SystemMessage::MSG_POWERUP, nullptr);
        systemAwake = true;
        if (config->powerOutputPin != 255) systemIO.setDigitalOutput(config->powerOutputPin, true);
    }
}

void PowerController::snooze()
{
    if (systemAwake)
    {
        //freshly going to sleep, send notification to all devices
        deviceManager.sendMessage(DeviceType::DEVICE_ANY, ANYDEVICE, SystemMessage::MSG_POWERDOWN, nullptr);
        systemAwake = false;
        countdown = 10; //give system some time to shut down
    }
}

/*
 * Load the device configuration.
 * If possible values are read from EEPROM. If not, reasonable default values
 * are chosen and the configuration is overwritten in the EEPROM.
 */
void PowerController::loadConfiguration() {
    config = (PowerConfiguration *) getConfiguration();

    if (!config) { // as lowest sub-class make sure we have a config object
        config = new PowerConfiguration();
        Logger::debug("loading configuration in light controller");
        setConfiguration(config);
    }
    
    Device::loadConfiguration(); // call parent

    prefsHandler->read("OutPin", &config->powerOutputPin, 255);
    prefsHandler->read("CANBit", &config->powerTriggerCANBit, 255);
    prefsHandler->read("CANID", &config->powerTriggerCANID, 0x7FF);
    prefsHandler->read("TriggerPin", &config->powerTriggerPin, 255);
}

/*
 * Store the current configuration to EEPROM
 */
void PowerController::saveConfiguration() {
    config = (PowerConfiguration *) getConfiguration();

    Device::saveConfiguration(); // call parent
    prefsHandler->write("OutPin", config->powerOutputPin);
    prefsHandler->write("CANBit", config->powerTriggerCANBit);
    prefsHandler->write("CANID", config->powerTriggerCANID);
    prefsHandler->write("TriggerPin", config->powerTriggerPin);

    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}

DMAMEM PowerController powerCtrl;
