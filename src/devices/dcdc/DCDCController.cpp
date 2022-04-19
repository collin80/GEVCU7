/*
 * Delphi DC-DC Converter Controller.cpp
 *
 * CAN Interface to the Delphi DC-DC converter - Handles sending of commands and reception of status frames to drive the DC-DC converter and set its output voltage.  SB following.
 *
Copyright (c) 2014 Jack Rickard

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



#include "DCDCController.h"
#include "../../DeviceManager.h"

template<class T> inline Print &operator <<(Print &obj, T arg) {
    obj.print(arg);
    return obj;
}

DCDCController::DCDCController() : Device()
{
    outputVoltage = 0.0f;
    outputCurrent = 0.0f;
    deviceTemperature = 0.0f;
    isEnabled = false;
    isFaulted = false;
}

void DCDCController::setup()
{
    DCDCConfiguration *config = (DCDCConfiguration *)getConfiguration();

    Device::setup(); // run the parent class version of this function

    ConfigEntry entry;
    entry = {"DC-TARGETV", "Target output voltage for DC/DC", &config->targetLowVoltage, CFG_ENTRY_VAR_TYPE::FLOAT, 0.0f, 1000.0f, 2, nullptr};
    cfgEntries.push_back(entry);
    entry = {"DC-REQHVREADY", "Enable DC/DC only when HV is ready? (0=No, 1=Yes)", &config->requireHVReady, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"DC-ENABLEPIN", "Output pin to use to enable DC/DC (255 if not needed)", &config->enablePin, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);

    StatusEntry stat;
    //        name              var         type                  prevVal  obj
    stat = {"DC_OutputV", &outputVoltage, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"DC_OutputC", &outputCurrent, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"DC_Temperature", &deviceTemperature, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
}

void DCDCController::handleTick() 
{
    Device::handleTick(); //kick the ball up to papa
}

DeviceType DCDCController::getType()
{
    return (DeviceType::DEVICE_DCDC);
}

float DCDCController::getOutputVoltage()
{
    return outputVoltage;
}

float DCDCController::getOutputCurrent()
{
    return outputCurrent;
}

float DCDCController::getTemperature()
{
    return deviceTemperature;
}

void DCDCController::loadConfiguration()
{
    DCDCConfiguration *config = (DCDCConfiguration *)getConfiguration();

    if (!config) {
        config = new DCDCConfiguration();
        setConfiguration(config);
    }

    Device::loadConfiguration(); // call parent

    prefsHandler->read("TargetVoltage", &config->targetLowVoltage, 13.5f);
    prefsHandler->read("ReqHVReady", &config->requireHVReady, 1);
    prefsHandler->read("EnablePin", &config->enablePin, 255);
}

void DCDCController::saveConfiguration()
{
    DCDCConfiguration *config = (DCDCConfiguration *)getConfiguration();

    Device::saveConfiguration();

    prefsHandler->write("TargetVoltage", config->targetLowVoltage);
    prefsHandler->write("ReqHVReady", config->requireHVReady);
    prefsHandler->write("EnablePin", config->enablePin);    

    prefsHandler->saveChecksum();
}

