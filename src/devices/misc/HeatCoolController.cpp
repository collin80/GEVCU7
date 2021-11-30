/*
 * HeatCoolController.h - implements a simple unified way to control fans, coolant pumps, heaters, etc that
 might be used in a car. 
 
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

#include "HeatCoolController.h"

/*
 * Constructor
 */
HeatCoolController::HeatCoolController() : Device() {    
    commonName = "Heating and Cooling Controller";
    shortName = "HeatCool";
    isHeatOn = false;
    isPumpOn = false;
    for (int i = 0; i < COOL_ZONES; i++) isCoolOn[i] = false;
}

void HeatCoolController::earlyInit()
{
    prefsHandler = new PrefHandler(HEATCOOL);
}

/*
 * Setup the device.
 */
void HeatCoolController::setup() {
    tickHandler.detach(this); // unregister from TickHandler first

    Logger::info("add device: HeatCoolControl (id: %X, %X)", HEATCOOL, this);

    loadConfiguration();

    Device::setup(); //call base class

    HeatCoolConfiguration *config = (HeatCoolConfiguration *)getConfiguration();

    cfgEntries.reserve(8 + 4 * COOL_ZONES);

    ConfigEntry entry;
    entry = {"HEATONTEMP", "Temperature at which to enable heater (C)", &config->heatOnTemperature, CFG_ENTRY_VAR_TYPE::FLOAT, -10.0f, 100.0f, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"HEATOFFTEMP", "Temperature at which to cease heating", &config->heatOffTemperature, CFG_ENTRY_VAR_TYPE::FLOAT, -10.0f, 100.0f, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"HEATPIN", "Output used to trigger heating (255=Disabled)", &config->heatEnablePin, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);


    entry = {"PUMPHEAT", "Should water pump be active when heating is on? (0=no, 1=yes)", &config->runPumpWithHeat, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PUMPREADY", "Should water pump be active when system is on? (0=no, 1=yes)", &config->runPumpAtSysReady, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PUMPPIN", "Output used to trigger water pump (255=Disabled)", &config->waterPumpPin, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);

    char buff[30];
    for (int i = 0; i < COOL_ZONES; i++)
    {
        snprintf(buff, 30, "COOLONTEMP%u", i);
        entry = {buff, "Temperature at which zone cooling is turned on", &config->coolOnTemperature[i], CFG_ENTRY_VAR_TYPE::FLOAT, -10.0f, 200.0f};
        cfgEntries.push_back(entry);
        snprintf(buff, 30, "COOLOFFTEMP%u", i);
        entry = {buff, "Temperature at which zone cooling is turned off", &config->coolOffTemperature[i], CFG_ENTRY_VAR_TYPE::FLOAT, -10.0f, 200.0f};
        cfgEntries.push_back(entry);
        snprintf(buff, 30, "COOLZONETYPE%u", i);
        entry = {buff, "Where does this zone get temperature from (0=MotorCtrl, 1=BMS, 2=DCDC)", &config->coolZoneType[i], CFG_ENTRY_VAR_TYPE::BYTE, 0, 2};
        cfgEntries.push_back(entry);
        snprintf(buff, 30, "COOLPIN%u", i);
        entry = {buff, "Output used for this zone (255=Disabled)", &config->coolPins[i], CFG_ENTRY_VAR_TYPE::BYTE, 0, 255};
        cfgEntries.push_back(entry);
    }

    tickHandler.attach(this, CFG_TICK_INTERVAL_HEATCOOL);
}

/*
 * Process a timer event. This is where you should be doing checks and updates. 
 */
void HeatCoolController::handleTick() {
    Device::handleTick(); // Call parent which controls the workflow
    Logger::debug("HeatCool Tick Handler");
    return;
    HeatCoolConfiguration *config = (HeatCoolConfiguration *) getConfiguration();

    if (!isPumpOn)
    {
        if (config->runPumpAtSysReady || (isHeatOn && config->runPumpWithHeat))
        {
            if (config->waterPumpPin != 255) systemIO.setDigitalOutput(config->waterPumpPin, true);
            isPumpOn = true;
        }
    }

    BatteryManager *bms = static_cast<BatteryManager *>(deviceManager.getDeviceByType(DeviceType::DEVICE_BMS));
    MotorController *mctl = static_cast<MotorController *>(deviceManager.getDeviceByType(DeviceType::DEVICE_MOTORCTRL));
    DCDCController *dcdc = static_cast<DCDCController *>(deviceManager.getDeviceByType(DeviceType::DEVICE_DCDC));    
    if (bms->hasTemperatures())
    {
        if (bms->getLowestTemperature() < config->heatOnTemperature)
        {
            if (!isHeatOn)
            {
                if (config->heatEnablePin != 255) systemIO.setDigitalOutput(config->heatEnablePin, true);
                isHeatOn = true;
            }
        }
        if (bms->getLowestTemperature() > config->heatOffTemperature)
        {
            if (isHeatOn)
            {
                if (config->heatEnablePin != 255) systemIO.setDigitalOutput(config->heatEnablePin, false);
                isHeatOn = false;
                if (isPumpOn && !config->runPumpAtSysReady)
                {
                    if (config->waterPumpPin != 255) systemIO.setDigitalOutput(config->waterPumpPin, false);
                    isPumpOn = false;
                }
            }
        }
    }

    for (int i = 0; i < COOL_ZONES; i++)
    {
        if (config->coolPins[i] == 255) continue; //no need to do processing if no pin is set up
        float temperature = 0.0f;
        switch (config->coolZoneType[i])
        {
        case CZ_BMS:
            temperature = bms->getHighestTemperature();
            break;
        case CZ_MOTORCTRL:
            temperature = mctl->getTemperatureInverter();
            if (mctl->getTemperatureSystem() > temperature) temperature = mctl->getTemperatureSystem();
            if (mctl->getTemperatureMotor() > temperature) temperature = mctl->getTemperatureMotor();
            temperature /= 10.0f; //all returned temperatures above were in 10th of a degree
            break;
        case CZ_DCDC:
            //doesn't actually report temperature at the moment.
            break;
        }
        if (temperature > config->coolOnTemperature[i])
        {
            if (!isCoolOn[i])
            {
                isCoolOn[i] = true;
                systemIO.setDigitalOutput(config->coolPins[i], true);
            }
        }
        if (temperature < config->coolOffTemperature[i])
        {
            if (isCoolOn[i])
            {
                isCoolOn[i] = false;
                systemIO.setDigitalOutput(config->coolPins[i], false);
            }
        }
    }
}
 
/*
 * Return the device ID
 */
DeviceId HeatCoolController::getId() {
    return (HEATCOOL);
}

DeviceType HeatCoolController::getType()
{
    return DEVICE_MISC;
}

/*
 * Load the device configuration.
 * If possible values are read from EEPROM. If not, reasonable default values
 * are chosen and the configuration is overwritten in the EEPROM.
 */
void HeatCoolController::loadConfiguration() {
    HeatCoolConfiguration *config = (HeatCoolConfiguration *) getConfiguration();

    if (!config) { // as lowest sub-class make sure we have a config object
        config = new HeatCoolConfiguration();
        Logger::debug("loading configuration in HeatCool");
        setConfiguration(config);
    }
    
    Device::loadConfiguration(); // call parent

    //first the stuff there is only one of
    prefsHandler->read("heatOnT", &config->heatOnTemperature, 1.0f);
    prefsHandler->read("heatOffT", &config->heatOffTemperature, 10.0f);
    prefsHandler->read("runPumpWithHeat", &config->runPumpWithHeat, 0);
    prefsHandler->read("runPumpAtReady", &config->runPumpAtSysReady, 1);
    prefsHandler->read("heatPin", &config->heatEnablePin, 255);
    prefsHandler->read("waterPumpPin", &config->waterPumpPin, 255);

    //these all are contingent on what COOL_ZONES is set to.
    char buff[30];
    for (int i = 0; i < COOL_ZONES; i++)
    {
        snprintf(buff, 30, "coolOnTemp%u", i);
        prefsHandler->read(buff, &config->coolOnTemperature[i], 60.0f);
        snprintf(buff, 30, "coolOffTemp%u", i);
        prefsHandler->read(buff, &config->coolOffTemperature[i], 50.0f);
        snprintf(buff, 30, "coolZoneType%u", i);
        prefsHandler->read(buff, (uint8_t *)&config->coolZoneType[i], CZ_BMS);
        snprintf(buff, 30, "coolPin%u", i);
        prefsHandler->read(buff, &config->coolPins[i], 255);
    }
}

/*
 * Store the current configuration to EEPROM
 */
void HeatCoolController::saveConfiguration() {
    HeatCoolConfiguration *config = (HeatCoolConfiguration *) getConfiguration();

    Device::saveConfiguration(); // call parent

    //first the stuff there is only one of
    prefsHandler->write("heatOnT", config->heatOnTemperature);
    prefsHandler->write("heatOffT", config->heatOffTemperature);
    prefsHandler->write("runPumpWithHeat", config->runPumpWithHeat);
    prefsHandler->write("runPumpAtReady", config->runPumpAtSysReady);
    prefsHandler->write("heatPin", config->heatEnablePin);
    prefsHandler->write("waterPumpPin", config->waterPumpPin);

    //these all are contingent on what COOL_ZONES is set to.
    char buff[30];
    for (int i = 0; i < COOL_ZONES; i++)
    {
        snprintf(buff, 30, "coolOnTemp%u", i);
        prefsHandler->write(buff, config->coolOnTemperature[i]);
        snprintf(buff, 30, "coolOffTemp%u", i);
        prefsHandler->write(buff, config->coolOffTemperature[i]);
        snprintf(buff, 30, "coolZoneType%u", i);
        prefsHandler->write(buff, (uint8_t)config->coolZoneType[i]);
        snprintf(buff, 30, "coolPin%u", i);
        prefsHandler->write(buff, config->coolPins[i]);
    }

    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}

HeatCoolController heatcoolController;
