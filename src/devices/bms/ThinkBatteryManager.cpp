/*
 * ThinkBatteryManager.cpp
 *
 * Interface to the BMS which is within the Think City battery packs
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

#include "ThinkBatteryManager.h"

ThinkBatteryManager::ThinkBatteryManager() : BatteryManager() {
    allowCharge = false;
    allowDischarge = false;
    commonName = "Think City BMS";
    shortName = "ThinkBMS";
}

void ThinkBatteryManager::earlyInit()
{
    prefsHandler = new PrefHandler(THINKBMS);
}

void ThinkBatteryManager::setup() {
    tickHandler.detach(this);

    Logger::info("add device: Th!nk City BMS (id: %X, %X)", THINKBMS, this);

    loadConfiguration();

    BatteryManager::setup(); // run the parent class version of this function

    ThinkBatteryManagerConfiguration *config = (ThinkBatteryManagerConfiguration *)getConfiguration();

    ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type                   Min Max Precision Funct
    entry = {"THINK-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr};
    cfgEntries.push_back(entry);

    setAttachedCANBus(config->canbusNum);

    //Relevant BMS messages are 0x300 - 0x30F
    attachedCANBus->attach(this, 0x300, 0x7f0, false);

    tickHandler.attach(this, CFG_TICK_INTERVAL_BMS_THINK);
    crashHandler.addBreadcrumb(ENCODE_BREAD("THBMS") + 0);
}

/*For all multibyte integers the format is MSB first, LSB last
*/
void ThinkBatteryManager::handleCanFrame(const CAN_message_t &frame) {
    int temp;
    crashHandler.addBreadcrumb(ENCODE_BREAD("THBMS") + 1);
    switch (frame.id) {
    case 0x300: //Start up message
        //we're not really interested in much here except whether init worked.
        if ((frame.buf[6] & 1) == 0)  //there was an initialization error!
        {
            faultHandler.raiseFault(THINKBMS, FAULT_BMS_INIT, true);
            allowCharge = false;
            allowDischarge = false;
        }
        else
        {
            faultHandler.cancelOngoingFault(THINKBMS, FAULT_BMS_INIT);
        }
        break;
    case 0x301: //System Data 0
        //first two bytes = current, next two voltage, next two DOD, last two avg. temp
        //readings in tenths
        packVoltage = (frame.buf[0] * 256 + frame.buf[1]) / 10.0f;
        packCurrent = (frame.buf[2] * 256 + frame.buf[3]) / 10.0f;
        break;
    case 0x302: //System Data 1
        if ((frame.buf[0] & 1) == 1) //Byte 0 bit 0 = general error
        {
            faultHandler.raiseFault(THINKBMS, FAULT_BMS_MISC, true);
            allowDischarge = false;
            allowCharge = false;
        }
        else
        {
            faultHandler.cancelOngoingFault(THINKBMS, FAULT_BMS_MISC);
        }
        if ((frame.buf[2] & 1) == 1) //Byte 2 bit 0 = general isolation error
        {
            faultHandler.raiseFault(THINKBMS, FAULT_HV_BATT_ISOLATION, true);
            allowDischarge = false;
            allowCharge = false;
        }
        else
        {
            faultHandler.cancelOngoingFault(THINKBMS, FAULT_HV_BATT_ISOLATION);
        }
        //Min discharge voltage = bytes 4-5 - tenths of a volt
        //Max discharge current = bytes 6-7 - tenths of an amp
        temp = (int16_t)(frame.buf[6] * 256 + frame.buf[7]);
        if (temp > 0) allowDischarge = true;
        break;
    case 0x303: //System Data 2
        //bytes 0-1 = max charge voltage (tenths of volt)
        //bytes 2-3 = max charge current (tenths of amp)
        temp = (int16_t)(frame.buf[2] * 256 + frame.buf[3]);
        if (temp > 0) allowCharge = true;
        //byte 4 bit 1 = regen braking OK, bit 2 = Discharging OK
        //byte 6 bit 3 = EPO (emergency power off) happened, bit 5 = battery pack fan is on
        break;
    case 0x304: //System Data 3
        //Byte 2 lower 4 bits = highest error category
        //categories: 0 = no faults, 1 = Reserved, 2 = Warning, 3 = Delayed switch off, 4 = immediate switch off
        //bytes 4-5 = Pack max temperature (tenths of degree C) - Signed
        //byte 6-7 = Pack min temperature (tenths of a degree C) - Signed
        lowestCellTemp = ((int16_t)(frame.buf[4] * 256 + frame.buf[5])) / 10.0f;
        highestCellTemp = ((int16_t)(frame.buf[6] * 256 + frame.buf[7])) / 10.0f;
        break;
    case 0x305: //System Data 4
        //byte 2 bits 0-3 = BMS state
        //0 = idle state, 1 = discharge state (contactor closed), 15 = fault state
        //byte 2 bit 4 = Internal HV isolation fault
        //byte 2 bit 5 = External HV isolation fault
        break;
    case 0x306: //System Data 5
        //bytes 0-1 = Equiv. internal resistance in milliohms
        //not recommended to rely on so probably just ignore it
        break;
        //technically there is a specification for frames 0x307 - 0x30A but I have never seen these frames
        //sent on the canbus system so I doubt that they are used.
        /*
        	case 0x307: //System Data 6
        	case 0x308: //System Data 7
        	case 0x309: //System Data 8
        	case 0x30A: //System Data 9
        	//do we care about the serial #? Probably not.
        	case 0x30E: //Serial # part 1
        	case 0x30B: //Serial # part 2
        */
    }
    crashHandler.addBreadcrumb(ENCODE_BREAD("THBMS") + 2);
}

void ThinkBatteryManager::handleTick() {
    BatteryManager::handleTick(); //kick the ball up to papa

    sendKeepAlive();

}

//Contactors in pack will close if we sent these two frames with all zeros.
void ThinkBatteryManager::sendKeepAlive()
{
    CAN_message_t output;
    output.len = 3;
    output.id = 0x310;
    output.flags.extended = 0; //standard frame
    for (int i = 0; i < 8; i++) output.buf[i] = 0;
    attachedCANBus->sendFrame(output);

    output.id = 0x311;
    output.len = 2;
    attachedCANBus->sendFrame(output);
    crashHandler.addBreadcrumb(ENCODE_BREAD("THBMS") + 3);
}

DeviceId ThinkBatteryManager::getId()
{
    return (THINKBMS);
}

bool ThinkBatteryManager::hasPackVoltage()
{
    return true;
}

bool ThinkBatteryManager::hasPackCurrent()
{
    return true;
}

bool ThinkBatteryManager::hasLimits()
{
    return false;
}

bool ThinkBatteryManager::hasTemperatures()
{
    return true;
}

bool ThinkBatteryManager::isChargeOK()
{
    return allowCharge;
}

bool ThinkBatteryManager::isDischargeOK()
{
    return allowDischarge;
}

void ThinkBatteryManager::loadConfiguration() {
    ThinkBatteryManagerConfiguration *config = (ThinkBatteryManagerConfiguration *)getConfiguration();

    if (!config) {
        config = new ThinkBatteryManagerConfiguration();
        setConfiguration(config);
    }

    BatteryManager::loadConfiguration(); // call parent

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
}

void ThinkBatteryManager::saveConfiguration() {
    ThinkBatteryManagerConfiguration *config = (ThinkBatteryManagerConfiguration *)getConfiguration();

    if (!config) {
        config = new ThinkBatteryManagerConfiguration();
        setConfiguration(config);
    }
    prefsHandler->write("CanbusNum", config->canbusNum);
    BatteryManager::saveConfiguration();
}

ThinkBatteryManager thinkBMS;


