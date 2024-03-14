/*
 * OrionBatteryManager.cpp
 *
 * Interface to OrionBMS (gen 2)
 *
Copyright (c) 2023 Collin Kidder, Michael Neuweiler, Charles Galpin

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

#include "OrionBatteryManager.h"

OrionBatteryManager::OrionBatteryManager() : BatteryManager() {
    allowCharge = false;
    allowDischarge = false;
    commonName = "Orion BMS";
    shortName = "OrionBMS";
}

void OrionBatteryManager::earlyInit()
{
    prefsHandler = new PrefHandler(ORIONBMS);
}

void OrionBatteryManager::setup() {
    tickHandler.detach(this);

    Logger::info("add device: Orion BMS (id: %X, %X)", ORIONBMS, this);

    loadConfiguration();

    BatteryManager::setup(); // run the parent class version of this function

    OrionBatteryManagerConfiguration *config = (OrionBatteryManagerConfiguration *)getConfiguration();

    ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type                   Min Max Precision Funct
    entry = {"ORION-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr};
    cfgEntries.push_back(entry);

    setAttachedCANBus(config->canbusNum);

    attachedCANBus->attach(this, 0x6B0, 0x7f0, false);

    setAlive();

    tickHandler.attach(this, CFG_TICK_INTERVAL_BMS_ORION);
    crashHandler.addBreadcrumb(ENCODE_BREAD("ORBMS") + 0);
}

/*For all multibyte integers the format is MSB first, LSB last
*/
void OrionBatteryManager::handleCanFrame(const CAN_message_t &frame) {
    int temp;
    int16_t curr;
    crashHandler.addBreadcrumb(ENCODE_BREAD("ORBMS") + 1);
    setAlive();
    switch (frame.id) {
    case 0x6B0: //status msg 1 - current, voltage, SOC
        packVoltage = (frame.buf[2] * 256 + frame.buf[3]) / 10.0f;
        curr = (frame.buf[0] * 256 + frame.buf[1]);
        packCurrent =  curr / 10.0f;
        SOC = frame.buf[4] * 0.5;
        break;
    case 0x6B1: //status msg 2 - temperatures and limits
        dischargeLimit = (frame.buf[0] * 256) + frame.buf[1];
        chargeLimit = frame.buf[2];
        lowestCellTemp = frame.buf[5];
        highestCellTemp = frame.buf[4];
        break;
    }
    crashHandler.addBreadcrumb(ENCODE_BREAD("ORBMS") + 2);
}

//might think of tracking messages from OrionBMS and faulting if they stop. This would be a good idea!
void OrionBatteryManager::handleTick() {
    BatteryManager::handleTick(); //kick the ball up to papa
    checkAlive(4000);
    //sendKeepAlive();

}

DeviceId OrionBatteryManager::getId()
{
    return (ORIONBMS);
}

bool OrionBatteryManager::hasPackVoltage()
{
    return true;
}

bool OrionBatteryManager::hasPackCurrent()
{
    return true;
}

bool OrionBatteryManager::hasTemperatures()
{
    return true;
}

bool OrionBatteryManager::hasLimits()
{
    return true;
}

bool OrionBatteryManager::isChargeOK()
{
    return allowCharge;
}

bool OrionBatteryManager::isDischargeOK()
{
    return allowDischarge;
}

void OrionBatteryManager::loadConfiguration() {
    OrionBatteryManagerConfiguration *config = (OrionBatteryManagerConfiguration *)getConfiguration();

    if (!config) {
        config = new OrionBatteryManagerConfiguration();
        setConfiguration(config);
    }

    BatteryManager::loadConfiguration(); // call parent

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
}

void OrionBatteryManager::saveConfiguration() {
    OrionBatteryManagerConfiguration *config = (OrionBatteryManagerConfiguration *)getConfiguration();

    if (!config) {
        config = new OrionBatteryManagerConfiguration();
        setConfiguration(config);
    }
    prefsHandler->write("CanbusNum", config->canbusNum);
    BatteryManager::saveConfiguration();
}

DMAMEM OrionBatteryManager orionBMS;


