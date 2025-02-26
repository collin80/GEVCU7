/*
 * Device.cpp
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

#include "Device.h"
#include "../DeviceManager.h"

Device::Device() {
    deviceConfiguration = nullptr;
    prefsHandler = nullptr;
    //since all derived classes eventually call this base method this will cause every device to auto register itself with the device manager
    deviceManager.addDevice(this);
    commonName = "Generic Device";
    shortName = "GENDEV";
}

//Empty functions to handle these callbacks if the derived classes don't

void Device::setup() 
{
}

void Device::earlyInit()
{
    if (!prefsHandler) prefsHandler = new PrefHandler(deviceId);
}

//when called this will unregister the device so it quits getting updates
//declared virtual in case any devices need to do more teardown.
void Device::disableDevice()
{
    tickHandler.detach(this); //no longer receive ticks

    //technically this should be a dynamic cast to see if the object really is a CAN observer
    //but no-rtti is enabled so I can't do that. It should be OK anyway as nothing from the actual
    //CanObserver class is being called. The memory address is all we need
    CanObserver *obs = (CanObserver *)(this);
    if (obs) //if this device was a can observer then cancel those too.
    {
        canHandlerBus0.detachAll(obs);
        canHandlerBus1.detachAll(obs);
        canHandlerBus2.detachAll(obs);
    }
    //send disable message to the device in case it wants to do anything fancy
    deviceManager.sendMessage(DEVICE_ANY, this->getId(), MSG_DISABLE, NULL);
}

const char* Device::getCommonName() {
    return commonName;
}

const char* Device::getShortName() {
    return shortName;
}

void Device::handleTick() {
}

uint32_t Device::getTickInterval() {
    return 0;
}

//just bubbles up the value from the preference handler.
bool Device::isEnabled() {
    return prefsHandler->isEnabled();
}

void Device::forceEnableState(bool state)
{
    if (prefsHandler)
    {
        prefsHandler->setEnabledStatus(state);
    }
}

void Device::handleMessage(uint32_t msgType, const void* message) {
    switch (msgType) {
    case MSG_STARTUP:
        this->earlyInit();
        break;
    case MSG_SETUP:
        this->setup();
        //send enable message to the device in case it wants to do anything fancy
        deviceManager.sendMessage(DEVICE_ANY, this->getId(), MSG_ENABLE, NULL);
        break;
    }
}

void Device::loadConfiguration() {
}

void Device::saveConfiguration() {
}

DeviceConfiguration *Device::getConfiguration() {
    //if (!this->deviceConfiguration) this->loadConfiguration(); //try to load configuration if it hasn't been done yet.
    return this->deviceConfiguration;
}

void Device::setConfiguration(DeviceConfiguration *configuration) {
    this->deviceConfiguration = configuration;
}

const std::vector<ConfigEntry> *Device::getConfigEntries()
{
    return &cfgEntries;
}

void Device::zapConfiguration()
{
    prefsHandler->resetEEPROM();
    loadConfiguration(); //then try to reload configuration to bring it back to defaults
}

const ConfigEntry* Device::findConfigEntry(const char *settingName)
{
    for (size_t idx = 0; idx < cfgEntries.size(); idx++)
    {
        //Serial.printf("%s %s\n", settingName, ent.cfgName.c_str());
        if (!strcmp(settingName, cfgEntries.at(idx).cfgName.c_str()))
        {
            //Serial.printf("Found it. Address is %x\n", &entries->at(idx));
            return &cfgEntries.at(idx);
        }
    }
    return nullptr;
}

const char* Device::getFaultDescription(uint16_t faultcode)
{
    if (faultcode < LAST_FAULT_CODE) return DEVICE_FAULT_DESCS[faultcode];
    return nullptr;
}

DeviceId Device::getId()
{
    return deviceId;
}

DeviceType Device::getType()
{
    return deviceType;
}



