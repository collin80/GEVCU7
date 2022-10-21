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

extern std::vector<ConfigEntry> sysCfgEntries;

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
}

void SerialConsole::loop() {
  
    if (handlingEvent == false) {
        while (SerialUSB.available()) {
            serialEvent();
        }
    }
}

void SerialConsole::printConfigEntry(const Device *dev, const ConfigEntry &entry)
{
    String str = "   ";
    str += entry.cfgName + "=";

    String descString;
    const char *descPtr = nullptr;
    if (entry.descFunc)
    {
        //TODO: Anyone know how to squash the compiler warning for this next line? I give up...
        descString = CALL_MEMBER_FN(dev, entry.descFunc)();
        descPtr = descString.c_str();
    }

    switch (entry.varType)
    {
    case CFG_ENTRY_VAR_TYPE::BYTE:
        if (entry.precision == 16) str += "0x%X";
        else str += "%u";
        if (descPtr)
        {
            str += " [%s] - " + entry.helpText;
            Logger::console(str.c_str(), *(uint8_t *)entry.varPtr, descPtr);
        }
        else 
        {
            str += " - " + entry.helpText;
            Logger::console(str.c_str(), *(uint8_t *)entry.varPtr);
        }
        break;
    case CFG_ENTRY_VAR_TYPE::FLOAT:
        char formatString[20];
        if (descPtr)
        {
            snprintf(formatString, 20, "%%.%uf [%%s] - ", entry.precision);            
            str += formatString + entry.helpText;
            Logger::console(str.c_str(), *(float *)entry.varPtr, descPtr);
        }
        else 
        {
            snprintf(formatString, 20, "%%.%uf - ", entry.precision);
            str += formatString + entry.helpText;
            Logger::console(str.c_str(), *(float *)entry.varPtr);
        }
        break;
    case CFG_ENTRY_VAR_TYPE::INT16:
        if (descPtr)
        {
            str += "%i [%s] - " + entry.helpText;
            Logger::console(str.c_str(), *(int16_t *)entry.varPtr, descPtr);
        }
        else 
        {
            str += "%i - " + entry.helpText;
            Logger::console(str.c_str(), *(int16_t *)entry.varPtr);
        }
        break;
    case CFG_ENTRY_VAR_TYPE::INT32:
        if (entry.descFunc)
        {
            str += "%i [%s] - " + entry.helpText;
            Logger::console(str.c_str(), *(int32_t *)entry.varPtr, descPtr);
        }
        else 
        {
            str += "%i - " + entry.helpText;
            Logger::console(str.c_str(), *(int32_t *)entry.varPtr);
        }
        break;
    case CFG_ENTRY_VAR_TYPE::STRING:
        str += "%s - " + entry.helpText;
        Logger::console(str.c_str(), (char *)entry.varPtr);
        break;
    case CFG_ENTRY_VAR_TYPE::UINT16:
        if (entry.precision == 16) str += "0x%X";
        else str += "%u";
        if (descPtr)
        {
            str += " [%s] - " + entry.helpText;
            Logger::console(str.c_str(), *(uint16_t *)entry.varPtr, descPtr);
        }
        else 
        {
            str += " - " + entry.helpText;
            Logger::console(str.c_str(), *(uint16_t *)entry.varPtr);
        }
        break;
    case CFG_ENTRY_VAR_TYPE::UINT32:
        if (entry.precision == 16) str += "0x%X";
        else str += "%u";
        if (descPtr)
        {
            str += " [%s] - " + entry.helpText;
            Logger::console(str.c_str(), *(uint32_t *)entry.varPtr, descPtr);
        }
        else 
        {
            str += " - " + entry.helpText;
            Logger::console(str.c_str(), *(uint32_t *)entry.varPtr);
        }
        break;    
    }
}

void SerialConsole::getConfigEntriesForDevice(Device *dev)
{
    Logger::console("\n\n%s Configuration", dev->getCommonName());
    const std::vector<ConfigEntry> *entries = dev->getConfigEntries();
    for (const ConfigEntry &ent : *entries)
    {
        printConfigEntry(dev, ent);
    }
}

//have to run through all devices registered and for each enabled device
//check whether it has the settingName in its configuration entries
//If so process the config entry and return. Otherwise keep going.
//doesn't yet actually tell the device to update EEPROM and doesn't output
//anything yet if setting was a success. But, it's getting there.
void SerialConsole::updateSetting(const char *settingName, char *valu)
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
        if (ui8 < entry->minValue.u_int) result = 1;
        else if (ui8 > entry->maxValue.u_int) result = 2;
        else *(uint8_t *)entry->varPtr = ui8;
        break;
    case CFG_ENTRY_VAR_TYPE::FLOAT:
        fl = strtof(valu, NULL);
        if (fl < entry->minValue.floating) result = 1;
        else if (fl > entry->maxValue.floating) result = 2;
        else *(float *)entry->varPtr = fl;
        break;
    case CFG_ENTRY_VAR_TYPE::INT16:
        i16 = (int16_t)strtol(valu, NULL, 0);
        if (i16 < entry->minValue.s_int) result = 1;
        else if (i16 > entry->maxValue.s_int) result = 2;
        else *(int16_t *)entry->varPtr = i16;
        break;
    case CFG_ENTRY_VAR_TYPE::INT32:
        i32 = (int32_t)strtol(valu, NULL, 0);
        if (i32 < entry->minValue.s_int) result = 1;
        else if (i32 > entry->maxValue.s_int) result = 2;
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
        if (ui16 < entry->minValue.u_int) result = 1;
        else if (ui16 > entry->maxValue.u_int) result = 2;
        else *(uint16_t *)entry->varPtr = ui16;
        break;
    case CFG_ENTRY_VAR_TYPE::UINT32:
        ui32 = (uint32_t)strtol(valu, NULL, 0);
        if (ui32 < entry->minValue.u_int) result = 1;
        else if (ui32 > entry->maxValue.u_int) result = 2;
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
        Logger::console("Value was below minimum value of %f for parameter %s", entry->minValue, settingName);
    }
    if (result == 2) //value was too high
    {
        Logger::console("Value was above maximum value of %f for parameter %s", entry->maxValue, settingName);
    }
}


void SerialConsole::printMenu() {
    MotorController* motorController = (MotorController*) deviceManager.getMotorController();
    Throttle *accelerator = deviceManager.getAccelerator();
    Throttle *brake = deviceManager.getBrake();

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
  
    Logger::console("\nDEVICE SELECTION AND ACTIVATION\n");
    Logger::console("     q = Dump Device Table");
    Logger::console("     Q = Reinitialize device table");
    Logger::console("     S = show possible device IDs");
    Logger::console("     NUKE=1 - Resets all device settings in EEPROM. You have been warned.");

    deviceManager.printDeviceList();

    for (int j = 0; j < CFG_DEV_MGR_MAX_DEVICES; j++)
    {
        Device *dev = deviceManager.getDeviceByIdx(j);
        if (dev)
        {
            if (dev->isEnabled()) getConfigEntriesForDevice(dev);
            if (dev == accelerator)
            {
                Logger::console("   z = detect throttle min/max, num throttles and subtype");
                Logger::console("   Z = save throttle values");
            }
            if (dev == brake)
            {
                Logger::console("   b = detect brake min/max");
                Logger::console("   B = save brake values");
            }
        }
    }
      
    Logger::console("\nANALOG AND DIGITAL IO\n");
    Logger::console("   A = Autocompensate ADC inputs");
    Logger::console("   J = set all digital outputs low");
    Logger::console("   K = set all digital outputs high");
 
    if (heartbeat != NULL) {
        Logger::console("   L = show raw analog/digital input/output values (toggle)");
    }
    Logger::console("   OUTPUT=<0-7> - toggles state of specified digital output");
   
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
    int i;
    int newValue;
    bool updateWifi = true;

    //Logger::debug("Cmd size: %i", ptrBuffer);
    if (ptrBuffer < 6)
        return; //4 digit command, =, value is at least 6 characters
    cmdBuffer[ptrBuffer] = 0; //make sure to null terminate
    String cmdString = String();
    char *strVal;
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

    // strtol() is able to parse also hex values (e.g. a string "0xCAFE"), useful for enable/disable by device id
    newValue = strtol((char *) (cmdBuffer + i), NULL, 0);
    strVal = (char *)(cmdBuffer + i); //leave it as a string

    cmdString.toUpperCase();
    
    //most all config stuff is done via a generic interface now. So for device settings there is 
    //nothing here any longer. The call to updateSetting handles all that now.

/*} else */if (cmdString == String("ENABLE")) {
        if (PrefHandler::setDeviceStatus(newValue, true)) {
            memCache->FlushAllPages();
            Logger::console("Successfully enabled device.(%X, %d) Trying to start it immediately!", newValue, newValue);
            Device *dev = deviceManager.getDeviceByID(newValue);
            if (dev) dev->setup();
        }
        else {
            Logger::console("Invalid device ID (%X, %d)", newValue, newValue);
        }
    } else if (cmdString == String("DISABLE")) {
        if (PrefHandler::setDeviceStatus(newValue, false)) {
            memCache->FlushAllPages();
            Logger::console("Successfully disabled device. Trying to stop it immediately.");
            Device *dev = deviceManager.getDeviceByID(newValue);
            if (dev) dev->disableDevice();
        }
        else {
            Logger::console("Invalid device ID (%X, %d)", newValue, newValue);
        }
    } else if (cmdString == String("ZAPDEV")) {
        Device *dev = deviceManager.getDeviceByID(newValue);
        if (dev)
        {
            Logger::console("Zapping configuration space for ID %x", newValue);
            dev->zapConfiguration();
        }
        else 
        {
            Logger::console("Invalid device ID (%X, %d)", newValue, newValue);
        }
    } else if (cmdString == String("OUTPUT") && newValue<8) {
        int outie = systemIO.getDigitalOutput(newValue);
        Logger::console("DOUT%d,  STATE: %d",newValue, outie);
        if(outie)
        {
            systemIO.setDigitalOutput(newValue,0);
            //motorController->statusBitfield1 &= ~(1 << newValue);//Clear
        }
        else
        {
            systemIO.setDigitalOutput(newValue,1);
            //motorController->statusBitfield1 |=1 << newValue;//setbit to Turn on annunciator
        }

        Logger::console("DOUT0:%d, DOUT1:%d, DOUT2:%d, DOUT3:%d, DOUT4:%d, DOUT5:%d, DOUT6:%d, DOUT7:%d", 
                        systemIO.getDigitalOutput(0), systemIO.getDigitalOutput(1), systemIO.getDigitalOutput(2), systemIO.getDigitalOutput(3), 
                        systemIO.getDigitalOutput(4), systemIO.getDigitalOutput(5), systemIO.getDigitalOutput(6), systemIO.getDigitalOutput(7));

    } else if (cmdString == String("NUKE")) {
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
    //MotorController* motorController = (MotorController*) deviceManager.getMotorController();
    Throttle *accelerator = deviceManager.getAccelerator();
    Throttle *brake = deviceManager.getBrake();
    Device *sysDev = deviceManager.getDeviceByID(SYSTEM);

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
        for (int i = 0; i < 7; i++)
        {
            systemIO.calibrateADCOffset(i, true);
        }        
        sysDev->saveConfiguration();
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


