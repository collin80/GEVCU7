/*
 * PotGearSelector.cpp
 
   Monitor an ADC pin and use it to select which gear we should be in.
 
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

#include "PotGearSelector.h"

PotGearSelector::PotGearSelector() : Device()
{
    shortName = "PotGear";
    commonName = "Potentiometer Gear Selector";
    deviceId = POTGEARSEL;
    deviceType = DEVICE_MISC;
}

void PotGearSelector::setup()
{
    tickHandler.detach(this);

    Logger::info("add device: Pot Gear Selector (id:%X, %X)", POTGEARSEL, this);

    PotGearSelConfiguration *config = (PotGearSelConfiguration *)getConfiguration();

    ConfigEntry entry;
    entry = {"PGADC", "Set ADC for pot based gear selector", &config->adcPin, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PGHYST", "Set hysteresis for gear signal", &config->hysteresis, CFG_ENTRY_VAR_TYPE::UINT16, 0, 2048, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PGPARK", "Set nominal value for park position", &config->parkPosition, CFG_ENTRY_VAR_TYPE::UINT16, 0, 4096, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PGREV", "Set nominal value for reverse position", &config->reversePosition, CFG_ENTRY_VAR_TYPE::UINT16, 0, 4096, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PGNEU", "Set nominal value for neutral position", &config->neutralPosition, CFG_ENTRY_VAR_TYPE::UINT16, 0, 4096, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"PGDRIVE", "Set nominal value for drive position", &config->drivePosition, CFG_ENTRY_VAR_TYPE::UINT16, 0, 4096, 0, nullptr};
    cfgEntries.push_back(entry);

    loadConfiguration();

    Device::setup(); // run the parent class version of this function

    tickHandler.attach(this, TICK_POTGEAR);
}

void PotGearSelector::handleTick() {
    PotGearSelConfiguration *config = (PotGearSelConfiguration *)getConfiguration();
    MotorController* motorController = (MotorController*) deviceManager.getMotorController();

    Device::handleTick(); //kick the ball up to papa

    if (config->adcPin == 255) return;

    int16_t gearSelector = systemIO.getAnalogIn(config->adcPin);

    bool foundGearPos = false;

    if (gearSelector > (config->parkPosition - config->hysteresis) &&  gearSelector < (config->parkPosition + config->hysteresis))
    {
        Logger::debug("Setting gear to Park(neutral)");
        if (motorController) motorController->setSelectedGear(MotorController::NEUTRAL);
        foundGearPos = true;
    }

    if (gearSelector > (config->neutralPosition - config->hysteresis) &&  gearSelector < (config->neutralPosition + config->hysteresis))
    {
        Logger::debug("Setting gear to neutral");
        if (motorController) motorController->setSelectedGear(MotorController::NEUTRAL);
        foundGearPos = true;
    }

    if (gearSelector > (config->drivePosition - config->hysteresis) &&  gearSelector < (config->drivePosition + config->hysteresis))
    {
        Logger::debug("Setting gear to drive");
        if (motorController) motorController->setSelectedGear(MotorController::DRIVE);
        foundGearPos = true;
    }

    if (gearSelector > (config->reversePosition - config->hysteresis) &&  gearSelector < (config->reversePosition + config->hysteresis))
    {
        Logger::debug("Setting gear to reverse");
        if (motorController) motorController->setSelectedGear(MotorController::REVERSE);
        foundGearPos = true;
    }

    if (!foundGearPos)
    {
        Logger::debug("Gear selector ADC out of bounds! Is it misconfigured?");
    }
}

uint32_t PotGearSelector::getTickInterval()
{
    return TICK_POTGEAR;
}

void PotGearSelector::loadConfiguration()
{
    PotGearSelConfiguration *config = (PotGearSelConfiguration *)getConfiguration();

    if (!config) {
        config = new PotGearSelConfiguration();
        setConfiguration(config);
    }

    Device::loadConfiguration(); // call parent

    //if (prefsHandler->checksumValid()) { //checksum is good, read in the values stored in EEPROM
        //Logger::info((char *)Constants::validChecksum);
        prefsHandler->read("AdcPin", &config->adcPin, 2);
        prefsHandler->read("GearPark", &config->parkPosition, 50);
        prefsHandler->read("GearReverse", &config->reversePosition, 250);
        prefsHandler->read("GearNeutral", &config->neutralPosition, 450);
        prefsHandler->read("GearDrive", &config->drivePosition, 850);
        prefsHandler->read("Hysteresis", &config->hysteresis, 100);
    //}
}

void PotGearSelector::saveConfiguration()
{
    PotGearSelConfiguration *config = (PotGearSelConfiguration *)getConfiguration();

    Device::saveConfiguration();

    prefsHandler->write("AdcPin", config->adcPin);
    prefsHandler->write("GearPark", config->parkPosition);
    prefsHandler->write("GearReverse", config->reversePosition);
    prefsHandler->write("GearNeutral", config->neutralPosition);
    prefsHandler->write("GearDrive", config->drivePosition);
    prefsHandler->write("Hysteresis", config->hysteresis);
    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}

DMAMEM PotGearSelector potgear;