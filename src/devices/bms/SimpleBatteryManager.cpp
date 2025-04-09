/*
 * SimpleBatteryManager.cpp
 *
 * Checks voltage and current from various devices in the system and
 * attempts to turn this into a round about BMS of sorts that can
 * track the state of charge. No promises here, the SOC calculations
 * could be far off but they probably will suffice some some uses.
 *
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

#include "SimpleBatteryManager.h"
#include "../charger/ChargeController.h"
#include "../dcdc/DCDCController.h"

SimpleBatteryManager::SimpleBatteryManager() : BatteryManager() {
    allowCharge = true;
    allowDischarge = true;
    commonName = "Simple BMS";
    shortName = "SimpleBMS";
    deviceId = SIMPLEBMS;
    firstReading = true;
    lastMS = 0;
}

void SimpleBatteryManager::setup() {
    tickHandler.detach(this);

    Logger::info("add device: Th!nk City BMS (id: %X, %X)", SIMPLEBMS, this);

    loadConfiguration();

    BatteryManager::setup(); // run the parent class version of this function

    ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type          Min Max Precision Funct
    entry = {"SBMS-NOMAH", "Nominal AH capacity of the pack", &config->nominalPackAH, CFG_ENTRY_VAR_TYPE::FLOAT, {.floating = 0.0}, {.floating = 2000.0}, 1, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"SBMS-CURRAH", "Nominal AH capacity of the pack", &config->currentPackAH, CFG_ENTRY_VAR_TYPE::FLOAT, {.floating = 0.0}, {.floating = 2000.0}, 1, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"SBMS-EMPTYV", "Nominal AH capacity of the pack", &config->packEmptyVoltage, CFG_ENTRY_VAR_TYPE::FLOAT, {.floating = 0.0}, {.floating = 2000.0}, 1, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"SBMS-FULLV", "Nominal AH capacity of the pack", &config->packFullVoltage, CFG_ENTRY_VAR_TYPE::FLOAT, {.floating = 0.0}, {.floating = 2000.0}, 1, nullptr, nullptr};
    cfgEntries.push_back(entry);

    tickHandler.attach(this, CFG_TICK_INTERVAL_BMS_SIMPLE);
    crashHandler.addBreadcrumb(ENCODE_BREAD("SIBMS") + 0);
}

void SimpleBatteryManager::handleTick() {
    BatteryManager::handleTick(); //kick the ball up to papa
    MotorController *mc = deviceManager.getMotorController();
    ChargeController *cc = (ChargeController *)deviceManager.getDeviceByType(DeviceType::DEVICE_CHARGER);
    DCDCController *dc = (DCDCController *)deviceManager.getDeviceByType(DeviceType::DEVICE_DCDC);

    float targetSOC;

    if (!firstReading)
    {
        float totalCurrent = 0;
        if (mc) totalCurrent += mc->getDcCurrent();
        if (cc) totalCurrent -= cc->getOutputCurrent();

        float v_interval = (config->packFullVoltage - config->packEmptyVoltage) / 5;
        float upperVBound = config->packFullVoltage - v_interval;
        float lowerVBound = config->packEmptyVoltage + v_interval;

        targetSOC = SOC;

        //assume upper and lower 20% are linear in voltage and try to adjust SOC to match
        float dcV = mc->getDcVoltage();
        if (dcV < lowerVBound)
        {
            targetSOC = 20.0f - ((lowerVBound - dcV) / v_interval * 20.0f);
            if (targetSOC < 0.0f) targetSOC = 0.0f;
    
            //only intervene if the difference is more than 2%
            if (fabs(targetSOC - SOC) > 2.0f )
            {
                totalCurrent += 1000.0; //pretend to be using a lot of current to knock down the SOC
            }
        }
        else if (dcV > upperVBound)
        {
            targetSOC = 80.0f + ((dcV - upperVBound) / v_interval * 20.0f);
            if (targetSOC > 100.0f) targetSOC = 100.0f;
    
            if (fabs(targetSOC - SOC) > 2.0f)
            {
                totalCurrent -= 1000.0;
            }
        }

        if (SOC < 2.0f) allowDischarge = false;
            else allowDischarge = true;
        if (SOC > 99.5f) allowCharge = false;
            else allowCharge = true;

        uint32_t interval = millis() - lastMS;
        lastMS = millis();
        //an hour is 3.6 million milliseconds
        float ah_partial = interval / 3600000.0f;
        ah_partial *= totalCurrent;
        config->currentPackAH -= ah_partial;
        SOC = (config->currentPackAH / config->nominalPackAH) * 100.0f;
        Logger::debug("Target SOC: %f       Real SOC: %f        dcv: %f", targetSOC, SOC, dcV);
        Logger::debug("Charging OK: %i     Discharging OK: %i", allowCharge, allowDischarge);
        //write the parameter but don't force the cache. Let it naturally time out every so often
        //and save to EEPROM. This limits the write cycles to EEPROM.
        prefsHandler->write("CurrAH", config->currentPackAH);

    }
    else
    {
        firstReading = false;
        lastMS = millis();
    }
}

bool SimpleBatteryManager::hasPackVoltage()
{
    return true;
}

bool SimpleBatteryManager::hasPackCurrent()
{
    return true;
}

bool SimpleBatteryManager::hasLimits()
{
    return false;
}

bool SimpleBatteryManager::hasTemperatures()
{
    return false;
}

bool SimpleBatteryManager::isChargeOK()
{
    return allowCharge;
}

bool SimpleBatteryManager::isDischargeOK()
{
    return allowDischarge;
}

void SimpleBatteryManager::loadConfiguration() {
    config = (SimpleBatteryManagerConfiguration *)getConfiguration();

    if (!config) {
        config = new SimpleBatteryManagerConfiguration();
        setConfiguration(config);
    }

    BatteryManager::loadConfiguration(); // call parent

    prefsHandler->read("NomAH", &config->nominalPackAH, 100.0f);
    prefsHandler->read("CurrAH", &config->currentPackAH, 100.0f);
    prefsHandler->read("EmptyV", &config->packEmptyVoltage, 250.0f);
    prefsHandler->read("FullV", &config->packFullVoltage, 400.0f);
}

void SimpleBatteryManager::saveConfiguration() {
    config = (SimpleBatteryManagerConfiguration *)getConfiguration();

    if (!config) {
        config = new SimpleBatteryManagerConfiguration();
        setConfiguration(config);
    }
    prefsHandler->write("NomAH", config->nominalPackAH);
    prefsHandler->write("CurrAH", config->currentPackAH);
    prefsHandler->write("EmptyV", config->packEmptyVoltage);
    prefsHandler->write("FullV", config->packFullVoltage);
    BatteryManager::saveConfiguration();
}

DMAMEM SimpleBatteryManager simpleBMS;


