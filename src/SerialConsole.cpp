/*
 * SerialConsole.cpp
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

#include "SerialConsole.h"
template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; } //Lets us stream SerialUSB

extern PrefHandler *sysPrefs;

uint8_t systype;

SerialConsole::SerialConsole(MemCache* memCache) :
    memCache(memCache), heartbeat(NULL) {
    init();
}

SerialConsole::SerialConsole(MemCache* memCache, Heartbeat* heartbeat) :
    memCache(memCache), heartbeat(heartbeat) {
    init();
}

void SerialConsole::init() {
    handlingEvent = false;

    //State variables for serial console
    ptrBuffer = 0;
    state = STATE_ROOT_MENU;
    loopcount=0;
    cancel=false;

    sysPrefs->read("SysType", &systype, 7);
}

void SerialConsole::loop() {
  
    if (handlingEvent == false) {
        if (SerialUSB.available()) {
            serialEvent();
        }
    }
}

void SerialConsole::printConfigEntry(const ConfigEntry &entry)
{
    String str = "   ";
    str += entry.cfgName + "=";
    switch (entry.varType)
    {
    case CFG_ENTRY_VAR_TYPE::BYTE:
        str += "%u - " + entry.helpText;
        Logger::console(str.c_str(), *(uint8_t *)entry.varPtr);
        break;
    case CFG_ENTRY_VAR_TYPE::FLOAT:
        str += "%f - " + entry.helpText;
        Logger::console(str.c_str(), *(float *)entry.varPtr);
        break;
    case CFG_ENTRY_VAR_TYPE::INT16:
        str += "%i - " + entry.helpText;
        Logger::console(str.c_str(), *(int16_t *)entry.varPtr);
        break;
    case CFG_ENTRY_VAR_TYPE::INT32:
        str += "%i - " + entry.helpText;
        Logger::console(str.c_str(), *(int32_t *)entry.varPtr);
        break;
    case CFG_ENTRY_VAR_TYPE::STRING:
        str += "%s - " + entry.helpText;
        Logger::console(str.c_str(), *(char *)entry.varPtr);
        break;
    case CFG_ENTRY_VAR_TYPE::UINT16:
        str += "%u - " + entry.helpText;
        Logger::console(str.c_str(), *(uint16_t *)entry.varPtr);
        break;
    case CFG_ENTRY_VAR_TYPE::UINT32:
        str += "%u - " + entry.helpText;
        Logger::console(str.c_str(), *(uint32_t *)entry.varPtr);
        break;    
    }
}

void SerialConsole::getConfigEntriesForDevice(Device *dev)
{
    const std::vector<ConfigEntry> *entries = dev->getConfigEntries();
    for (const ConfigEntry ent : *entries)
    {
        printConfigEntry(ent);
    }
}

//have to run through all devices registered and for each enabled device
//check whether it has the settingName in its configuration entries
//If so process the config entry and return. Otherwise keep going.
//doesn't yet actually tell the device to update EEPROM and doesn't output
//anything yet if setting was a success. But, it's getting there.
void SerialConsole::updateSetting(char *settingName, char *valu)
{
    Device *deviceMatched;
    const ConfigEntry *entry = deviceManager.findConfigEntry(settingName, &deviceMatched);
    uint8_t ui8;
    float fl;
    int16_t i16;
    int32_t i32;
    uint16_t ui16;
    uint32_t ui32;

    int result = 0;
    if (!entry)
    {
        Logger::console("No such configuration parameter exists!");
        return;
    }
    switch (entry->varType)
    {
    case CFG_ENTRY_VAR_TYPE::BYTE:
        ui8 = (uint8_t)strtol(valu, NULL, 0);
        if (ui8 < entry->minValue) result = 1;
        else if (ui8 > entry->maxValue) result = 2;
        else *(uint8_t *)entry->varPtr = ui8;
        break;
    case CFG_ENTRY_VAR_TYPE::FLOAT:
        fl = strtof(valu, NULL);
        if (fl < entry->minValue) result = 1;
        else if (fl > entry->maxValue) result = 2;
        else *(float *)entry->varPtr = fl;
        break;
    case CFG_ENTRY_VAR_TYPE::INT16:
        i16 = (int16_t)strtol(valu, NULL, 0);
        if (i16 < entry->minValue) result = 1;
        else if (i16 > entry->maxValue) result = 2;
        else *(int16_t *)entry->varPtr = i16;
        break;
    case CFG_ENTRY_VAR_TYPE::INT32:
        i32 = (int32_t)strtol(valu, NULL, 0);
        if (i32 < entry->minValue) result = 1;
        else if (i32 > entry->maxValue) result = 2;
        else *(int32_t *)entry->varPtr = i32;
        break;
    case CFG_ENTRY_VAR_TYPE::STRING:
        //this is an interesting one. It's easy in principle but we don't know
        //the actual size of the storage buffer so it could overwrite memory.
        //If this GEVCU thing gets popular it might be necessary to fix this
        //otherwise this is the entry point to smashing the stack for fun and profit.
        strcpy((char *)entry->varPtr, valu);
        break;
    case CFG_ENTRY_VAR_TYPE::UINT16:
        ui16 = (uint16_t)strtol(valu, NULL, 0);
        if (ui16 < entry->minValue) result = 1;
        else if (ui16 > entry->maxValue) result = 2;
        else *(uint16_t *)entry->varPtr = ui16;
        break;
    case CFG_ENTRY_VAR_TYPE::UINT32:
        ui32 = (uint32_t)strtol(valu, NULL, 0);
        if (ui32 < entry->minValue) result = 1;
        else if (ui32 > entry->maxValue) result = 2;
        else *(uint32_t *)entry->varPtr = ui32;
        break;    
    }
    if (result == 0) //value was stored
    {
        Logger::console("%s was set as value for parameter %s", valu, settingName);
        deviceMatched->saveConfiguration();
    }
    if (result == 1) //value was too low
    {
        Logger::console("Value was below minimum value of %f for parameter %s", entry->minValue, entry->cfgName);
    }
    if (result == 2) //value was too high
    {
        Logger::console("Value was above maximum value of %f for parameter %s", entry->maxValue, entry->cfgName);
    }
}


void SerialConsole::printMenu() {
    MotorController* motorController = (MotorController*) deviceManager.getMotorController();
    Throttle *accelerator = deviceManager.getAccelerator();
    Throttle *brake = deviceManager.getBrake();
    BatteryManager *bms = static_cast<BatteryManager *>(deviceManager.getDeviceByType(DEVICE_BMS));
   
    //Show build # here as well in case people are using the native port and don't get to see the start up messages
    Logger::console("Build number: %u", CFG_BUILD_NUM);
    if (motorController) 
    {
        Logger::console("Motor Controller Status: isRunning: %i  isFaulted: %i", 
            motorController->isRunning(), motorController->isFaulted());  
    }
    Logger::console("\n*************SYSTEM MENU *****************");
    Logger::console("Enable line endings of some sort (LF, CR, CRLF)");
    Logger::console("Most commands case sensitive\n");
    Logger::console("GENERAL SYSTEM CONFIGURATION\n");
    Logger::console("   E = dump system EEPROM values");
    Logger::console("   h = help (displays this message)");
  
    Logger::console("   LOGLEVEL=%i - set log level (0=debug, 1=info, 2=warn, 3=error, 4=off)", Logger::getLogLevel());
    Logger::console("   CAN0SPEED=%i - set first CAN bus speed (in thousands)", canHandlerEv.getBusSpeed() / 1000);
    Logger::console("   CAN1SPEED=%i - set second CAN bus speed (in thousands)", canHandlerCar.getBusSpeed() / 1000);
    Logger::console("   CAN2SPEED=%i - set third CAN bus speed (in thousands)", canHandlerCar2.getBusSpeed() / 1000);
    Logger::console("   SWCANSPEED=%i - set SingleWire CAN bus speed (in thousands)", canHandlerSingleWire.getBusSpeed() / 1000);

    Logger::console("\nDEVICE SELECTION AND ACTIVATION\n");
    Logger::console("     q = Dump Device Table");
    Logger::console("     Q = Reinitialize device table");
    Logger::console("     S = show possible device IDs");
    Logger::console("     NUKE=1 - Resets all device settings in EEPROM. You have been warned.");

    deviceManager.printDeviceList();
    
    if (motorController && motorController->getConfiguration()) {
        MotorControllerConfiguration *config = (MotorControllerConfiguration *) motorController->getConfiguration(); 
        Logger::console("\nPRECHARGE CONTROLS\n");
        Logger::console("   PREDELAY=%i - Precharge delay time in milliseconds ", config->prechargeR);
        Logger::console("   PRELAY=%i - Which output to use for precharge contactor (255 to disable)", config->prechargeRelay);
        Logger::console("   MRELAY=%i - Which output to use for main contactor (255 to disable)", config->mainContactorRelay);
          
        Logger::console("\nMOTOR CONTROLS\n");
        getConfigEntriesForDevice(motorController);

        Logger::console("\nOTHER VEHICLE CONTROLS\n");
        Logger::console("   COOLFAN=%i - Digital output to turn on cooling fan(0-7, 255 for none)", config->coolFan);
        Logger::console("   COOLON=%i - Inverter temperature C to turn cooling on", config->coolOn);
        Logger::console("   COOLOFF=%i - Inverter temperature C to turn cooling off", config->coolOff);
        Logger::console("   BRAKELT = %i - Digital output to turn on brakelight (0-7, 255 for none)", config->brakeLight);
        Logger::console("   REVLT=%i - Digital output to turn on reverse light (0-7, 255 for none)", config->revLight);  
        Logger::console("   NOMV=%i - Fully charged pack voltage that automatically resets kWh counter", config->nominalVolt/10);        
    }

    if (accelerator && accelerator->getConfiguration()) {
        Logger::console("\nTHROTTLE CONTROLS\n");
        Logger::console("   z = detect throttle min/max, num throttles and subtype");
        Logger::console("   Z = save throttle values");
        getConfigEntriesForDevice(accelerator);
    }

    if (brake && brake->getConfiguration()) {
        Logger::console("\nBRAKE CONTROLS\n");
        Logger::console("   b = detect brake min/max");
        Logger::console("   B = save brake values");
        getConfigEntriesForDevice(brake);
    }
    
    if (bms && bms->getConfiguration()) {
        BatteryManagerConfiguration *config = static_cast<BatteryManagerConfiguration *>(bms->getConfiguration());
        Logger::console("\nBATTERY MANAGEMENT CONTROLS\n");
        getConfigEntriesForDevice(bms);        
    }
  
    Logger::console("\nANALOG AND DIGITAL IO\n");
    Logger::console("   A = Autocompensate ADC inputs");
    Logger::console("   J = set all digital outputs low");
    Logger::console("   K = set all digital outputs high");
 
    if (heartbeat != NULL) {
        Logger::console("   L = show raw analog/digital input/output values (toggle)");
    }
    Logger::console("   OUTPUT=<0-7> - toggles state of specified digital output");
   
    uint16_t val;
    sysPrefs->read("Adc0Offset", &val, 0);
    Logger::console("   ADC0OFF=%i - set ADC0 offset", val);
    sysPrefs->read("Adc0Gain", &val, 1024);
    Logger::console("   ADC0GAIN=%i - set ADC0 gain (1024 is 1 gain)", val);
    sysPrefs->read("Adc1Offset", &val, 0);
    Logger::console("   ADC1OFF=%i - set ADC1 offset", val);
    sysPrefs->read("Adc1Gain", &val, 1024);
    Logger::console("   ADC1GAIN=%i - set ADC1 gain (1024 is 1 gain)", val);
    sysPrefs->read("Adc2Offset", &val, 0);
    Logger::console("   ADC2OFF=%i - set ADC2 offset", val);
    sysPrefs->read("Adc2Gain", &val, 1024);
    Logger::console("   ADC2GAIN=%i - set ADC2 gain (1024 is 1 gain)", val);
    sysPrefs->read("Adc3Offset", &val, 0);
    Logger::console("   ADC3OFF=%i - set ADC3 offset", val);
    sysPrefs->read("Adc3Gain", &val, 1024);
    Logger::console("   ADC3GAIN=%i - set ADC3 gain (1024 is 1 gain)", val);
    sysPrefs->read("Adc4Offset", &val, 0);
    Logger::console("   ADC4OFF=%i - set ADC4 offset", val);
    sysPrefs->read("Adc4Gain", &val, 1024);
    Logger::console("   ADC4GAIN=%i - set ADC4 gain (1024 is 1 gain)", val);
    sysPrefs->read("Adc5Offset", &val, 0);
    Logger::console("   ADC5OFF=%i - set ADC5 offset", val);
    sysPrefs->read("Adc5Gain", &val, 1024);
    Logger::console("   ADC5GAIN=%i - set ADC5 gain (1024 is 1 gain)", val);
    sysPrefs->read("Adc6Offset", &val, 0);
    Logger::console("   ADC6OFF=%i - set ADC6 offset", val);
    sysPrefs->read("Adc6Gain", &val, 1024);
    Logger::console("   ADC6GAIN=%i - set ADC6 gain (1024 is 1 gain)", val);
    sysPrefs->read("Adc7Offset", &val, 0);
    Logger::console("   ADC7OFF=%i - set ADC7 offset", val);
    sysPrefs->read("Adc7Gain", &val, 1024);
    Logger::console("   ADC7GAIN=%i - set ADC7 gain (1024 is 1 gain)", val);
}

/*	There is a help menu (press H or h or ?)

 This is no longer going to be a simple single character console.
 Now the system can handle up to 80 input characters. Commands are submitted
 by sending line ending (LF, CR, or both)
 */
void SerialConsole::serialEvent() {
    int incoming;
    incoming = SerialUSB.read();
    if (incoming == -1) { //false alarm....
        return;
    }

    if (incoming == 10 || incoming == 13) { //command done. Parse it.
        handleConsoleCmd();
        ptrBuffer = 0; //reset line counter once the line has been processed
    } else {
        cmdBuffer[ptrBuffer++] = (unsigned char) incoming;
        if (ptrBuffer > 79)
            ptrBuffer = 79;
    }
}

void SerialConsole::handleConsoleCmd() {
    handlingEvent = true;

    if (state == STATE_ROOT_MENU) {
        if (ptrBuffer == 1) { //command is a single ascii character
            handleShortCmd();
        } else { //if cmd over 1 char then assume (for now) that it is a config line
            handleConfigCmd();
        }
    }
    handlingEvent = false;
}

void SerialConsole::handleConfigCmd() {
    PotThrottleConfiguration *acceleratorConfig = NULL;
    PotThrottleConfiguration *brakeConfig = NULL;
    MotorControllerConfiguration *motorConfig = NULL;
    BatteryManagerConfiguration *bmsConfig = NULL;

    Throttle *accelerator = deviceManager.getAccelerator();
    Throttle *brake = deviceManager.getBrake();
    MotorController *motorController = deviceManager.getMotorController();
    BatteryManager *bms = static_cast<BatteryManager *>(deviceManager.getDeviceByType(DEVICE_BMS));
    int i;
    int newValue;
    bool updateWifi = true;

    //Logger::debug("Cmd size: %i", ptrBuffer);
    if (ptrBuffer < 6)
        return; //4 digit command, =, value is at least 6 characters
    cmdBuffer[ptrBuffer] = 0; //make sure to null terminate
    String cmdString = String();
    char *strVal;
    unsigned char whichEntry = '0';
    i = 0;

    while (cmdBuffer[i] != '=' && i < ptrBuffer) {
        cmdString.concat(String(cmdBuffer[i++]));
    }
    i++; //skip the =
    if (i >= ptrBuffer)
    {
        Logger::console("Command needs a value..ie TORQ=3000");
        Logger::console("");
        return; //or, we could use this to display the parameter instead of setting
    }

    if (accelerator)
        acceleratorConfig = (PotThrottleConfiguration *) accelerator->getConfiguration();
    if (brake)
        brakeConfig = (PotThrottleConfiguration *) brake->getConfiguration();
    if (motorController)
        motorConfig = (MotorControllerConfiguration *) motorController->getConfiguration();
    if (bms)
        bmsConfig = static_cast<BatteryManagerConfiguration *>(bms->getConfiguration());

    // strtol() is able to parse also hex values (e.g. a string "0xCAFE"), useful for enable/disable by device id
    newValue = strtol((char *) (cmdBuffer + i), NULL, 0);
    strVal = (char *)(cmdBuffer + i); //leave it as a string

    cmdString.toUpperCase();
    
    //most all config stuff is done via a generic interface now. So for device settings there is 
    //nothing here any longer. The call to updateSetting handles all that now.


/*    if (cmdString == String("TORQ") && motorConfig) {
        Logger::console("Setting Torque Limit to %i", newValue);
        motorConfig->torqueMax = newValue;
        motorController->saveConfiguration();
    } else if (cmdString == String("RPM") && motorConfig) {
        Logger::console("Setting RPM Limit to %i", newValue);
        motorConfig->speedMax = newValue;
        motorController->saveConfiguration();
    } else if (cmdString == String("REVLIM") && motorConfig) {
        Logger::console("Setting Reverse Limit to %i", newValue);
        motorConfig->reversePercent = newValue;
        motorController->saveConfiguration();
    } else if (cmdString == String("TPOT") && acceleratorConfig) {
        Logger::console("Setting # of Throttle Pots to %i", newValue);
        acceleratorConfig->numberPotMeters = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("TTYPE") && acceleratorConfig) {
        Logger::console("Setting Throttle Subtype to %i", newValue);
        acceleratorConfig->throttleSubType = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("T1ADC") && acceleratorConfig) {
        Logger::console("Setting Throttle1 ADC pin to %i", newValue);
        acceleratorConfig->AdcPin1 = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("T1MN") && acceleratorConfig) {
        Logger::console("Setting Throttle1 Min to %i", newValue);
        acceleratorConfig->minimumLevel1 = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("T1MX") && acceleratorConfig) {
        Logger::console("Setting Throttle1 Max to %i", newValue);
        acceleratorConfig->maximumLevel1 = newValue;
        accelerator->saveConfiguration();
    }
    else if (cmdString == String("T2ADC") && acceleratorConfig) {
        Logger::console("Setting Throttle2 ADC pin to %i", newValue);
        acceleratorConfig->AdcPin2 = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("T2MN") && acceleratorConfig) {
        Logger::console("Setting Throttle2 Min to %i", newValue);
        acceleratorConfig->minimumLevel2 = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("T2MX") && acceleratorConfig) {
        Logger::console("Setting Throttle2 Max to %i", newValue);
        acceleratorConfig->maximumLevel2 = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("TRGNMAX") && acceleratorConfig) {
        Logger::console("Setting Throttle Regen maximum to %i", newValue);
        acceleratorConfig->positionRegenMaximum = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("TRGNMIN") && acceleratorConfig) {
        Logger::console("Setting Throttle Regen minimum to %i", newValue);
        acceleratorConfig->positionRegenMinimum = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("TFWD") && acceleratorConfig) {
        Logger::console("Setting Throttle Forward Start to %i", newValue);
        acceleratorConfig->positionForwardMotionStart = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("TMAP") && acceleratorConfig) {
        Logger::console("Setting Throttle MAP Point to %i", newValue);
        acceleratorConfig->positionHalfPower = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("TMINRN") && acceleratorConfig) {
        Logger::console("Setting Throttle Regen Minimum Strength to %i", newValue);
        acceleratorConfig->minimumRegen = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("TMAXRN") && acceleratorConfig) {
        Logger::console("Setting Throttle Regen Maximum Strength to %i", newValue);
        acceleratorConfig->maximumRegen = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("TCREEP") && acceleratorConfig) {
        Logger::console("Setting Throttle Creep Strength to %i", newValue);
        acceleratorConfig->creep = newValue;
        accelerator->saveConfiguration();
    } else if (cmdString == String("BMAXR") && brakeConfig) {
        Logger::console("Setting Max Brake Regen to %i", newValue);
        brakeConfig->maximumRegen = newValue;
        brake->saveConfiguration();
    } else if (cmdString == String("BMINR") && brakeConfig) {
        Logger::console("Setting Min Brake Regen to %i", newValue);
        brakeConfig->minimumRegen = newValue;
        brake->saveConfiguration();
    }
    else if (cmdString == String("B1ADC") && brakeConfig) {
        Logger::console("Setting Brake ADC pin to %i", newValue);
        brakeConfig->AdcPin1 = newValue;
        brake->saveConfiguration();
    } else if (cmdString == String("B1MX") && brakeConfig) {
        Logger::console("Setting Brake Max to %i", newValue);
        brakeConfig->maximumLevel1 = newValue;
        brake->saveConfiguration();
    } else if (cmdString == String("B1MN") && brakeConfig) {
        Logger::console("Setting Brake Min to %i", newValue);
        brakeConfig->minimumLevel1 = newValue;
        brake->saveConfiguration();
    } else if (cmdString == String("PREC") && motorConfig) {
        Logger::console("Setting Precharge Capacitance to %i", newValue);
        motorConfig->kilowattHrs = newValue;
        motorController->saveConfiguration();
    } else if (cmdString == String("PREDELAY") && motorConfig) {
        Logger::console("Setting Precharge Time Delay to %i milliseconds", newValue);
        motorConfig->prechargeR = newValue;
        motorController->saveConfiguration();
    } else if (cmdString == String("NOMV") && motorConfig) {
        Logger::console("Setting fully charged voltage to %d vdc", newValue);
        motorConfig->nominalVolt = newValue * 10;
        motorController->saveConfiguration();
    } else if (cmdString == String("BRAKELT") && motorConfig) {
        motorConfig->brakeLight = newValue;
        motorController->saveConfiguration();
        Logger::console("Brake light output set to DOUT%i.",newValue);
    } else if (cmdString == String("REVLT") && motorConfig) {
        motorConfig->revLight = newValue;
        motorController->saveConfiguration();
        Logger::console("Reverse light output set to DOUT%i.",newValue);
    } else if (cmdString == String("ENABLEIN") && motorConfig) {
        motorConfig->enableIn = newValue;
        motorController->saveConfiguration();
        Logger::console("Motor Enable input set to DIN%i.",newValue);
    } else if (cmdString == String("REVIN") && motorConfig) {
        motorConfig->reverseIn = newValue;
        motorController->saveConfiguration();
        Logger::console("Motor Reverse input set to DIN%i.",newValue);
    } else if (cmdString == String("MRELAY") && motorConfig) {
        Logger::console("Setting Main Contactor relay output to DOUT%i", newValue);
        motorConfig->mainContactorRelay = newValue;
        motorController->saveConfiguration();
    } else if (cmdString == String("PRELAY") && motorConfig) {
        Logger::console("Setting Precharge Relay output to DOUT%i", newValue);
        motorConfig->prechargeRelay = newValue;
        motorController->saveConfiguration();
    } else if (cmdString == String("TAPERLO") && motorConfig) {
        if (newValue > -1 && newValue < 10001) {
            Logger::console("Setting taper lower limit to %i", newValue);
            motorConfig->regenTaperLower = newValue;
            motorController->saveConfiguration();
        }
        else Logger::console("Invalid RPM value. Please enter a value 0 to 10000");
    } else if (cmdString == String("TAPERHI") && motorConfig) {
        if (newValue >=  motorConfig->regenTaperLower && newValue < 10001) {
            Logger::console("Setting taper upper limit to %i", newValue);
            motorConfig->regenTaperUpper = newValue;
            motorController->saveConfiguration();
        }
        else Logger::console("Invalid RPM value. Please enter a value higher than low limit and under 10000");
    } else */if (cmdString == String("ENABLE")) {
        if (PrefHandler::setDeviceStatus(newValue, true)) {
            sysPrefs->saveChecksum();
            sysPrefs->forceCacheWrite(); //just in case someone takes us literally and power cycles quickly
            Logger::console("Successfully enabled device.(%X, %d) Power cycle to activate.", newValue, newValue);
        }
        else {
            Logger::console("Invalid device ID (%X, %d)", newValue, newValue);
        }
    } else if (cmdString == String("DISABLE")) {
        if (PrefHandler::setDeviceStatus(newValue, false)) {
            sysPrefs->saveChecksum();
            sysPrefs->forceCacheWrite(); //just in case someone takes us literally and power cycles quickly
            Logger::console("Successfully disabled device. Power cycle to deactivate.");
        }
        else {
            Logger::console("Invalid device ID (%X, %d)", newValue, newValue);
        }
    } else if (cmdString == String("SYSTYPE")) {
        if (newValue < 7 && newValue > 0) {
            sysPrefs->write("SysType", (uint8_t)(newValue));
            sysPrefs->saveChecksum();
            sysPrefs->forceCacheWrite(); //just in case someone takes us literally and power cycles quickly
            Logger::console("System type updated. Power cycle to apply.");
        }
        else Logger::console("Invalid system type. Please enter a value 1 - 4");
    } else if (cmdString == String("ADC0OFF")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc0Offset", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC0 Offset to %i", newValue);
        }
        else Logger::console("Invalid offset. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC0GAIN")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc0Gain", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC0 Gain to %i", newValue);
        }
        else Logger::console("Invalid gain. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC1OFF")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc1Offset", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC1 Offset to %i", newValue);
        }
        else Logger::console("Invalid offset. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC1GAIN")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc1Gain", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC1 Gain to %i", newValue);
        }
        else Logger::console("Invalid gain. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC2OFF")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc2Offset", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC2 Offset to %i", newValue);
        }
        else Logger::console("Invalid offset. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC2GAIN")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc2Gain", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC2 Gain to %i", newValue);
        }
        else Logger::console("Invalid gain. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC3OFF")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc3Offset", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC3 Offset to %i", newValue);
        }
        else Logger::console("Invalid offset. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC3GAIN")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc3Gain", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC3 Gain to %i", newValue);
        }
        else Logger::console("Invalid gain. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC4OFF")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc4Offset", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC4 Offset to %i", newValue);
        }
        else Logger::console("Invalid offset. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC4GAIN")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc4Gain", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC4 Gain to %i", newValue);
        }
        else Logger::console("Invalid gain. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC5OFF")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc5Offset", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC5 Offset to %i", newValue);
        }
        else Logger::console("Invalid offset. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC5GAIN")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc5Gain", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC5 Gain to %i", newValue);
        }
        else Logger::console("Invalid gain. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC6OFF")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc6Offset", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC6 Offset to %i", newValue);
        }
        else Logger::console("Invalid offset. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC6GAIN")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc6Gain", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC6 Gain to %i", newValue);
        }
        else Logger::console("Invalid gain. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC7OFF")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc7Offset", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC7 Offset to %i", newValue);
        }
        else Logger::console("Invalid offset. Enter value from 0 to 65535");
    } else if (cmdString == String("ADC7GAIN")) {
        if (newValue >= 0 && newValue <= 65535) {
            sysPrefs->write("Adc7Gain", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            systemIO.setup_ADC_params(); //change takes immediate effect
            Logger::console("Setting ADC7 Gain to %i", newValue);
        }
        else Logger::console("Invalid gain. Enter value from 0 to 65535");
    } else if (cmdString == String("CAN0SPEED")) {
        if (newValue >= 33 && newValue <= 1000) {
            sysPrefs->write("CAN0Speed", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            canHandlerEv.setup();
            Logger::console("Setting CAN0 speed to %i", newValue);
        }
        else Logger::console("Invalid speed. Enter a value between 33 and 1000");
    } else if (cmdString == String("CAN1SPEED")) {
        if (newValue >= 33 && newValue <= 1000) {
            sysPrefs->write("CAN1Speed", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            canHandlerCar.setup();
            Logger::console("Setting CAN1 speed to %i", newValue);
        }
        else Logger::console("Invalid speed. Enter a value between 33 and 1000");
    } else if (cmdString == String("CAN2SPEED")) {
        if (newValue >= 33 && newValue <= 1000) {
            sysPrefs->write("CAN2Speed", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            canHandlerCar2.setup();
            Logger::console("Setting CAN2 speed to %i", newValue);
        }
        else Logger::console("Invalid speed. Enter a value between 33 and 1000");
    } else if (cmdString == String("SWCANSPEED")) {
        if (newValue >= 33 && newValue <= 200) {
            sysPrefs->write("SWCANSpeed", (uint16_t)(newValue));
            sysPrefs->saveChecksum();
            canHandlerSingleWire.setup();
            Logger::console("Setting SWCAN speed to %i", newValue);
        }
        else Logger::console("Invalid speed. Enter a value between 33 and 1000");
    } else if (cmdString == String("LOGLEVEL")) {
        switch (newValue) {
        case 0:
            Logger::setLoglevel(Logger::Debug);
            Logger::console("setting loglevel to 'debug'");
            break;
        case 1:
            Logger::setLoglevel(Logger::Info);
            Logger::console("setting loglevel to 'info'");
            break;
        case 2:
            Logger::console("setting loglevel to 'warning'");
            Logger::setLoglevel(Logger::Warn);
            break;
        case 3:
            Logger::console("setting loglevel to 'error'");
            Logger::setLoglevel(Logger::Error);
            break;
        case 4:
            Logger::console("setting loglevel to 'off'");
            Logger::setLoglevel(Logger::Off);
            break;
        }
        if (!sysPrefs->write("LogLevel", (uint8_t)newValue))
            Logger::error("Couldn't write log level!");
        sysPrefs->saveChecksum();

   
    } else if (cmdString == String("COOLFAN") && motorConfig) {
        Logger::console("Cooling fan output updated to: %i", newValue);
        motorConfig->coolFan = newValue;
        motorController->saveConfiguration();
    } else if (cmdString == String("COOLON")&& motorConfig) {
        if (newValue <= 200 && newValue >= 0) {
            Logger::console("Cooling fan ON temperature updated to: %i degrees", newValue);
            motorConfig->coolOn = newValue;
            motorController->saveConfiguration();
        }
        else Logger::console("Invalid cooling ON temperature. Please enter a value 0 - 200F");
    } else if (cmdString == String("COOLOFF")&& motorConfig) {
        if (newValue <= 200 && newValue >= 0) {
            Logger::console("Cooling fan OFF temperature updated to: %i degrees", newValue);
            motorConfig->coolOff = newValue;
            motorController->saveConfiguration();
        }
        else Logger::console("Invalid cooling OFF temperature. Please enter a value 0 - 200F");
    } else if (cmdString == String("OUTPUT") && newValue<8) {
        int outie = systemIO.getDigitalOutput(newValue);
        Logger::console("DOUT%d,  STATE: %d",newValue, outie);
        if(outie)
        {
            systemIO.setDigitalOutput(newValue,0);
            motorController->statusBitfield1 &= ~(1 << newValue);//Clear
        }
        else
        {
            systemIO.setDigitalOutput(newValue,1);
            motorController->statusBitfield1 |=1 << newValue;//setbit to Turn on annunciator
        }


        Logger::console("DOUT0:%d, DOUT1:%d, DOUT2:%d, DOUT3:%d, DOUT4:%d, DOUT5:%d, DOUT6:%d, DOUT7:%d", 
                        systemIO.getDigitalOutput(0), systemIO.getDigitalOutput(1), systemIO.getDigitalOutput(2), systemIO.getDigitalOutput(3), 
                        systemIO.getDigitalOutput(4), systemIO.getDigitalOutput(5), systemIO.getDigitalOutput(6), systemIO.getDigitalOutput(7));

    } /*else if (cmdString == String("CAPACITY") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 6000) {
            bmsConfig->packCapacity = newValue;
            bms->saveConfiguration();
            Logger::console("Battery Pack Capacity set to: %d", bmsConfig->packCapacity);
        }
        else Logger::console("Invalid capacity please enter a value between 0 and 6000");
    } else if (cmdString == String("AHLEFT") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 6000) {
            bmsConfig->packAHRemaining = newValue * 100000ul;
            bms->saveConfiguration();
            Logger::console("Battery Pack remaining capacity set to: %d", newValue);
        }
        else Logger::console("Invalid remaining capacity please enter a value between 0 and 6000");
    } else if (cmdString == String("VOLTLIMHI") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 6000) {
            bmsConfig->highVoltLimit = newValue;
            bms->saveConfiguration();
            Logger::console("Battery High Voltage Limit set to: %d", bmsConfig->highVoltLimit);
        }
        else Logger::console("Invalid high voltage limit please enter a value between 0 and 6000");
    } else if (cmdString == String("VOLTLIMLO") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 6000) {
            bmsConfig->lowVoltLimit = newValue;
            bms->saveConfiguration();
            Logger::console("Battery Low Voltage Limit set to: %d", bmsConfig->lowVoltLimit);
        }
        else Logger::console("Invalid low voltage limit please enter a value between 0 and 6000");
    } else if (cmdString == String("CELLLIMHI") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 20000) {
            bmsConfig->highCellLimit = newValue;
            bms->saveConfiguration();
            Logger::console("Cell High Voltage Limit set to: %d", bmsConfig->highCellLimit);
        }
        else Logger::console("Invalid high voltage limit please enter a value between 0 and 20000");
    } else if (cmdString == String("CELLLIMLO") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 20000) {
            bmsConfig->lowCellLimit = newValue;
            bms->saveConfiguration();
            Logger::console("Cell Low Voltage Limit set to: %d", bmsConfig->lowCellLimit);
        }
        else Logger::console("Invalid low voltage limit please enter a value between 0 and 20000");
    } else if (cmdString == String("TEMPLIMHI") && bmsConfig ) {
        if (newValue >= 0 && newValue <= 2000) {
            bmsConfig->highTempLimit = newValue;
            bms->saveConfiguration();
            Logger::console("Battery Temperature Upper Limit set to: %d", bmsConfig->highTempLimit);
        }
        else Logger::console("Invalid temperature upper limit please enter a value between 0 and 2000");
    } else if (cmdString == String("TEMPLIMLO") && bmsConfig ) {
        if (newValue >= -2000 && newValue <= 2000) {
            bmsConfig->lowTempLimit = newValue;
            bms->saveConfiguration();
            Logger::console("Battery Temperature Lower Limit set to: %d", bmsConfig->lowTempLimit);
        }
        else Logger::console("Invalid temperature lower limit please enter a value between -2000 and 2000");
    } */else if (cmdString == String("NUKE")) {
        if (newValue == 1) {
            Logger::console("Start of EEPROM Nuke");
            memCache->InvalidateAll(); //first force writing of all dirty pages and invalidate them
            memCache->nukeFromOrbit(); //then completely erase EEPROM
            Logger::console("Device settings have been nuked. Reboot to reload default settings");
        }
    } else {
        //Logger::console("Unknown command");
        updateSetting(cmdString.c_str(), strVal);
        updateWifi = false;
    }
    // send updates to ichip wifi
    if (updateWifi) 
    {
        //deviceManager.sendMessage(DEVICE_WIFI, ICHIP2128, MSG_CONFIG_CHANGE, NULL);
        //deviceManager.sendMessage(DEVICE_WIFI, ADABLUE, MSG_CONFIG_CHANGE, NULL);
    }
}

void SerialConsole::handleShortCmd() {
    uint8_t val;
    MotorController* motorController = (MotorController*) deviceManager.getMotorController();
    Throttle *accelerator = deviceManager.getAccelerator();
    Throttle *brake = deviceManager.getBrake();

    switch (cmdBuffer[0]) {
    case 'h':
    case '?':
    case 'H':
        printMenu();
        break;
    case 'L':
        if (heartbeat != NULL) {
            heartbeat->setThrottleDebug(!heartbeat->getThrottleDebug());
            if (heartbeat->getThrottleDebug()) {
                Logger::console("Output raw throttle");
            } else {
                Logger::console("Cease raw throttle output");
            }
        }
        break;
    case 'E':
        Logger::console("Reading System EEPROM values");
        for (int i = 0; i < 256; i++) {
            memCache->Read(EE_SYSTEM_START + i, &val);
            Logger::console("%d: %d", i, val);
        }
        break;
    case 'K': //set all outputs high
        for (int tout = 0; tout < NUM_OUTPUT; tout++) systemIO.setDigitalOutput(tout, true);
        Logger::console("all outputs: ON");
        break;
    case 'J': //set the four outputs low
        for (int tout = 0; tout < NUM_OUTPUT; tout++) systemIO.setDigitalOutput(tout, false);
        Logger::console("all outputs: OFF");
        break;
    case 'z': // detect throttle min/max & other details
        if (accelerator) {
            ThrottleDetector *detector = new ThrottleDetector(accelerator);
            detector->detect();
        }
        break;
    case 'Z': // save throttle settings
        if (accelerator) {
            accelerator->saveConfiguration();
        }
        break;
    case 'b':
        if (brake) {
            ThrottleDetector *detector = new ThrottleDetector(brake);
            detector->detect();
        }
        break;
    case 'B':
        if (brake != NULL) {
            brake->saveConfiguration();
        }
        break;
    
    case 'A':
        uint32_t accum;
        for (int i = 0; i < 7; i++)
        {
            systemIO.calibrateADCOffset(i, true);
        }
        sysPrefs->saveChecksum();
        systemIO.setup_ADC_params(); //change takes immediate effect
        break;
    case 'a':
        //deviceManager.sendMessage(DEVICE_ANY, ADABLUE, 0xDEADBEEF, nullptr);
        break;
    case 'S': //generate a list of devices.
        deviceManager.printDeviceList();        
        break;
    
    case 'X':
        setup(); //this is probably a bad idea. Do not do this while connected to anything you care about - only for debugging in safety!
        break;
    case 'q':
        PrefHandler::dumpDeviceTable();
        break;
    case 'Q':
        PrefHandler::initDevTable();
        break;
    }
}


