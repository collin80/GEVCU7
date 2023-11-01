/*
 * Base class for HVAC climate control devices
 
Copyright (c) 2023 Collin Kidder

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

#include "HVACDevice.h"
#include "../../DeviceManager.h"

HVACController::HVACController() : Device()
{
    canHeat = false;
    canCool = false;
    isFaulted = false;
    currentTemperature = 0.0f;
    wattage = 0.0f;
}

void HVACController::setup()
{
    HVACConfiguration *config = (HVACConfiguration *)getConfiguration();

    Device::setup(); // run the parent class version of this function3

    ConfigEntry entry;
    entry = {"HVAC-TEMPERATURE", "Target climate temperature", &config->targetTemperature, CFG_ENTRY_VAR_TYPE::FLOAT, {.floating = 0.0}, {.floating = 100.0}, 2, nullptr};
    cfgEntries.push_back(entry);

    StatusEntry stat;
    //        name              var         type                  prevVal  obj
    stat = {"HVAC_ClimateTemp", &currentTemperature, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
}

void HVACController::handleTick()
{
    Device::handleTick(); //kick the ball up to papa
}

DeviceType HVACController::getType()
{
    return (DeviceType::DEVICE_HVAC);
}

double HVACController::getTemperature()
{
    return currentTemperature;
}

double HVACController::getWattage()
{
    return wattage;
}

void HVACController::loadConfiguration()
{
    HVACConfiguration *config = (HVACConfiguration *)getConfiguration();

    if (!config) {
        config = new HVACConfiguration();
        setConfiguration(config);
    }

    Device::loadConfiguration(); // call parent

    prefsHandler->read("TargetTemperature", &config->targetTemperature, 20.0f);
}

void HVACController::saveConfiguration()
{
    HVACConfiguration *config = (HVACConfiguration *)getConfiguration();

    Device::saveConfiguration();

    prefsHandler->write("TargetTemperature", config->targetTemperature);

    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}

