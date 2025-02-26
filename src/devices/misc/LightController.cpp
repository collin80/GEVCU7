/*
* LightController.cpp - Handles various lighting aspects that are probably required. Can trigger the reverse light
                        and the brake light based upon the state of the motor controller and the pedal and brake inputs
 
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

#include "LightController.h"

/*
 * Constructor
 */
LightController::LightController() : Device() {    
    commonName = "Light Controller";
    shortName = "LightCtrl";
    deviceType = DEVICE_MISC;
    deviceId = LIGHTCTRL;
}

/*
 * Setup the device.
 */
void LightController::setup() {
    //crashHandler.addBreadcrumb(ENCODE_BREAD("LITEC") + 0);
    tickHandler.detach(this); // unregister from TickHandler first

    Logger::info("add device: Light Controller (id: %X, %X)", LIGHTCTRL, this);

    loadConfiguration();

    Device::setup(); //call base class

    LightingConfiguration *config = (LightingConfiguration *)getConfiguration();

    cfgEntries.reserve(3);

    ConfigEntry entry;
    entry = {"BRAKELIGHT", "Set brake light output (255 = disabled)", &config->brakeLightOutput, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"REVLIGHT", "Set reverse light output (255 = disabled)", &config->reverseLightOutput, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"REQLEVEL","Required regen to trigger brake light (in 1/10 Nm) 0=Disabled", &config->reqRegenLevel, CFG_ENTRY_VAR_TYPE::INT16, {.s_int = -1000}, 0, 1, nullptr};
    cfgEntries.push_back(entry);

    tickHandler.attach(this, CFG_TICK_INTERVAL_LIGHTING);
}

/*
 * Process a timer event. This is where you should be doing checks and updates. 
 */
void LightController::handleTick() {
    //crashHandler.addBreadcrumb(ENCODE_BREAD("LITEC") + 1);
    Device::handleTick(); // Call parent which controls the workflow
    //Logger::avalanche("Lighting Tick Handler");

    LightingConfiguration *config = (LightingConfiguration *) getConfiguration();
    MotorController *mctl = static_cast<MotorController *>(deviceManager.getDeviceByType(DeviceType::DEVICE_MOTORCTRL));
    Throttle *throttle = static_cast<Throttle *>(deviceManager.getDeviceByType(DeviceType::DEVICE_THROTTLE));
    Throttle *brake = static_cast<Throttle *>(deviceManager.getDeviceByType(DeviceType::DEVICE_BRAKE));

    if (config->brakeLightOutput < 255)
    {
        if (mctl)
        {
            if (mctl->getSelectedGear() == MotorController::Gears::REVERSE)
            {
                systemIO.setDigitalOutput(config->brakeLightOutput, true);
            }
            else systemIO.setDigitalOutput(config->brakeLightOutput, false);
        }
    }

    if (config->reverseLightOutput < 255)
    {
        int16_t level = 0;
        if (throttle) level = throttle->getLevel();
        if (brake)
        {
            int16_t brklvl = brake->getLevel();
            if ( (brklvl < 0) && (brklvl < level) ) level = brklvl;
        }
        if (level < config->reqRegenLevel)
        {
            systemIO.setDigitalOutput(config->reverseLightOutput, true);
        }
        else systemIO.setDigitalOutput(config->reverseLightOutput, false);
    }
}

/*
 * Load the device configuration.
 * If possible values are read from EEPROM. If not, reasonable default values
 * are chosen and the configuration is overwritten in the EEPROM.
 */
void LightController::loadConfiguration() {
    LightingConfiguration *config = (LightingConfiguration *) getConfiguration();

    if (!config) { // as lowest sub-class make sure we have a config object
        config = new LightingConfiguration();
        Logger::debug("loading configuration in light controller");
        setConfiguration(config);
    }
    
    Device::loadConfiguration(); // call parent

    prefsHandler->read("BrakeLightOut", &config->brakeLightOutput, 255);
    prefsHandler->read("ReverseLightOut", &config->reverseLightOutput, 255);
    prefsHandler->read("ReqTorque", &config->reqRegenLevel, -30);
}

/*
 * Store the current configuration to EEPROM
 */
void LightController::saveConfiguration() {
    LightingConfiguration *config = (LightingConfiguration *) getConfiguration();

    Device::saveConfiguration(); // call parent
    prefsHandler->write("BrakeLightOut", config->brakeLightOutput);
    prefsHandler->write("ReverseLightOut", config->reverseLightOutput);
    prefsHandler->write("ReqTorque", config->reqRegenLevel);

    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}

DMAMEM LightController lightCtrl;