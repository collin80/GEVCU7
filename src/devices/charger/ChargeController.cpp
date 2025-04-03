/*
 * Base class for high voltage battery chargers
 *
Copyright (c) 2022 Collin Kidder

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



#include "ChargeController.h"
#include "../../DeviceManager.h"

template<class T> inline Print &operator <<(Print &obj, T arg) {
    obj.print(arg);
    return obj;
}

ChargeController::ChargeController() : Device()
{
    outputVoltage = 0.0f;
    outputCurrent = 0.0f;
    deviceTemperature = 0.0f;
    isEnabled = false;
    isFaulted = false;
    deviceType = DEVICE_CHARGER;
}

void ChargeController::setup()
{
    ChargeConfiguration *config = (ChargeConfiguration *)getConfiguration();

    Device::setup(); // run the parent class version of this function

    ConfigEntry entry;
    entry = {"CHARGER-TARGETV", "Target output voltage for charger", &config->targetUpperVoltage, CFG_ENTRY_VAR_TYPE::FLOAT, {.floating = 0.0}, {.floating = 1000.0}, 2, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"CHARGER-REQHVREADY", "Enable charger only when HV is ready? (0=No, 1=Yes)", &config->requireHVReady, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"CHARGER-ENABLEPIN", "Output pin to use to enable charger (255 if not needed)", &config->enablePin, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);

    StatusEntry stat;
    //        name              var         type                  prevVal  obj
    stat = {"CHGR_OutputV", &outputVoltage, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"CHGR_OutputC", &outputCurrent, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"CHGR_Temperature", &deviceTemperature, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
}

void ChargeController::handleTick() 
{
    Device::handleTick(); //kick the ball up to papa
}

float ChargeController::getOutputVoltage()
{
    return outputVoltage;
}

float ChargeController::getOutputCurrent()
{
    return outputCurrent;
}

float ChargeController::getTemperature()
{
    return deviceTemperature;
}

void ChargeController::loadConfiguration()
{
    ChargeConfiguration *config = (ChargeConfiguration *)getConfiguration();

    if (!config) {
        config = new ChargeConfiguration();
        setConfiguration(config);
    }

    Device::loadConfiguration(); // call parent

    prefsHandler->read("TargetVoltage", &config->targetUpperVoltage, 360.5f);
    prefsHandler->read("TargetAmps", &config->targetCurrentLimit, 20.0f);
    prefsHandler->read("ReqHVReady", &config->requireHVReady, 1);
    prefsHandler->read("EnablePin", &config->enablePin, 255);
}

void ChargeController::saveConfiguration()
{
    ChargeConfiguration *config = (ChargeConfiguration *)getConfiguration();

    Device::saveConfiguration();

    prefsHandler->write("TargetVoltage", config->targetUpperVoltage);
    prefsHandler->write("TargetAmps", config->targetCurrentLimit);
    prefsHandler->write("ReqHVReady", config->requireHVReady);
    prefsHandler->write("EnablePin", config->enablePin);    

    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}

const char* ChargeController::getFaultDescription(uint16_t faultcode)
{
    if ((faultcode >= 1000) && (faultcode < CHARGER_LAST_FAULT) ) return CHARGER_FAULT_DESCS[faultcode-1000];
    return Device::getFaultDescription(faultcode); //try generic device class if we couldn't handle it
    return nullptr; //no match, return nothing
}