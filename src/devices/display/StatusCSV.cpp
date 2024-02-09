/*
 * StatusCSV.cpp
 *
 * Create configurable status output on SerialUSB1
 *
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

#include "StatusCSV.h"
#include "CanHandler.h"

/*
 * Constructor.
 */
StatusCSV::StatusCSV() : Device()
{
    commonName = "Status output in CSV format";
    shortName = "StatusCSV";
    config = nullptr;
    tickCounter = 0;
    haveEnabledEntries = false;
    isEnabled = false;
}

void StatusCSV::earlyInit()
{
    prefsHandler = new PrefHandler(STATUSCSV);
}

/*
 * Initialization of hardware and parameters
 */
void StatusCSV::setup() {

    Logger::info("add device: StatusCSV (id: %X, %X)", STATUSCSV, this);

    tickHandler.detach(this);//Turn off tickhandler

    loadConfiguration(); //Retrieve any persistant variables

    Device::setup(); // run the parent class version of this function

    handleSerialSwitch(); //see if GVRET output should be turned off

    ConfigEntry entry;
    entry = {"TICKUPDATE", "Set number of timer ticks per update (20ms intervals)", &config->ticksPerUpdate, CFG_ENTRY_VAR_TYPE::UINT16, {.u_int = 1}, {.u_int = 10000}, 1, nullptr};
    cfgEntries.push_back(entry);
    entry = {"STATUS-EN", "Status entries to enable (or ALL for all of them)", &config->enableString, CFG_ENTRY_VAR_TYPE::STRING, {.u_int = 1}, {.u_int = 0xFFFFFFFF}, 1, nullptr};
    cfgEntries.push_back(entry);
    entry = {"STATUS-DIS", "Status entries to disable (or ALL)", &config->disableString, CFG_ENTRY_VAR_TYPE::STRING, {.u_int = 1}, {.u_int = 0xFFFFFFFF}, 1, nullptr};
    cfgEntries.push_back(entry);

    tickHandler.attach(this, CFG_TICK_INTERVAL_STATUS);

    config->enableString[0] = 0;
    config->disableString[0] = 0;
}

//This method handles periodic tick calls received from the tasker.
void StatusCSV::handleTick()
{
    StatusEntry *ent = nullptr;
    int c;

    if (config->enableString[0] > 0)
    {
        enableStatusHash(config->enableString);
        isEnabled = false;
        //Logger::console("Enabled the status entries!");
        saveConfiguration();
        config->enableString[0] = 0;
    }

    if (config->disableString[0] > 0)
    {
        disableStatusHash(config->disableString);
        isEnabled = false;
        //Logger::console("Disabled the status entries!");
        saveConfiguration();
        config->disableString[0] = 0;
    }

    while (SerialUSB1.available()) {
        c = SerialUSB1.read();
        if ((c == 's') || (c =='S'))
        {
            isEnabled = !isEnabled;
            if (isEnabled)
            {
                for (int i = 0; i < NUM_ENTRIES_IN_TABLE; i++)
                {
                    if (!config->enabledStatusEntries[i]) continue;
                    ent = deviceManager.findStatusEntryByHash(config->enabledStatusEntries[i]);
                    if (ent)
                    {
                        SerialUSB1.print(ent->statusName);
                        SerialUSB1.write(',');
                    }
                }
                SerialUSB1.println();
            }
        }
    }

    if (!isEnabled) return;
    if (++tickCounter > config->ticksPerUpdate)
    {
        tickCounter = 0;
        //do processing
        for (int i = 0; i < NUM_ENTRIES_IN_TABLE; i++)
        {
            if (!config->enabledStatusEntries[i]) continue;
            ent = deviceManager.findStatusEntryByHash(config->enabledStatusEntries[i]);
            if (ent)
            {
                SerialUSB1.print(ent->getValueAsString());
                SerialUSB1.write(',');
            }
        }
        SerialUSB1.println();
    }
}

void StatusCSV::enableStatusHash(char * str)
{
    if (!stricmp("ALL", str))
    {
//        for (int i = 0; i < NUM_ENTRIES_IN_TABLE; i++)
//        {
//            config->enabledStatusEntries[i] = 0;
//        }
        Logger::console("I lied. This is not supported yet... sorry....");
    }
    else
    {
        int idx;
        uint32_t hash;
        StatusEntry *ent = nullptr;
        char *tok = strtok(str, ",");
        while (tok)
        {
            idx = strtoul(tok, NULL, 0) - 1; //arrays are 0 based but indexes in program are 1 based
            ent = deviceManager.FindStatusEntryByIdx(idx);
            if (ent)
            {
                hash = ent->getHash();
                for (int i = 0; i < NUM_ENTRIES_IN_TABLE; i++)
                {
                    if (config->enabledStatusEntries[i] == 0)
                    {
                        config->enabledStatusEntries[i] = hash;
                        break;
                    }
                }
            }
            tok = strtok(NULL, ","); //get next one
        }
    }
    saveConfiguration();
    handleSerialSwitch();
}

void StatusCSV::disableStatusHash(char * str)
{
    if (!stricmp("ALL", str))
    {
        for (int i = 0; i < NUM_ENTRIES_IN_TABLE; i++)
        {
            config->enabledStatusEntries[i] = 0;
        }
    }
    else
    {
        int idx;
        uint32_t hash;
        StatusEntry *ent = nullptr;
        char *tok = strtok(str, ",");
        while (tok)
        {
            idx = strtoul(tok, NULL, 0) - 1;
            ent = deviceManager.FindStatusEntryByIdx(idx);
            if (ent)
            {
                hash = ent->getHash();
                for (int i = 0; i < NUM_ENTRIES_IN_TABLE; i++)
                {
                    if (config->enabledStatusEntries[i] == hash)
                    {
                        config->enabledStatusEntries[i] = 0;
                        break;
                    }
                }
            }
            tok = strtok(NULL, ","); //get next one
        }
    }
    saveConfiguration();
    handleSerialSwitch();
}

/*
The idea here is to see if we have any enabled status outputs. If we do and we've loaded this class then we will
make the second serial port our output. That means we have to turn off GVRET output on that serial port. Otherwise
we want to make sure GVRET mode is enable for that serial port.
This gives the code a dependency on CanHandler even though we're not sending any CAN (currently)
*/
void StatusCSV::handleSerialSwitch()
{
    bool anythingenabled = false;
    for (int i = 0; i < NUM_ENTRIES_IN_TABLE; i++)
    {
        if (config->enabledStatusEntries[i] > 0) anythingenabled = true;
    }
    if (anythingenabled)
    {
        canHandlerBus0.setGVRETMode(false);
        canHandlerBus1.setGVRETMode(false);
        canHandlerBus1.setGVRETMode(false);
        haveEnabledEntries = true;
    }
    else //this presupposes that we're the only one who could take over the second serial port. Currently that is correct.
    {
        canHandlerBus0.setGVRETMode(true);
        canHandlerBus1.setGVRETMode(true);
        canHandlerBus1.setGVRETMode(true);
        haveEnabledEntries = false;
    }
}

DeviceType StatusCSV::getType() {
    return DEVICE_MISC;
}

DeviceId StatusCSV::getId() {
    return (STATUSCSV);
}

bool StatusCSV::isHashMonitored(uint32_t hash)
{
    for (int i = 0; i < NUM_ENTRIES_IN_TABLE; i++)
    {
        if (config->enabledStatusEntries[i] == hash) return true;
    }
    return false;
}

void StatusCSV::loadConfiguration() {
    config = (StatusCSVConfiguration *)getConfiguration();

    if (!config) {
        config = new StatusCSVConfiguration();
        setConfiguration(config);
    }

    Device::loadConfiguration(); // call parent

    prefsHandler->read("TicksPer", &config->ticksPerUpdate, 5);
    //if block hasn't ever been written to EEPROM then just default it to all zeros
    if (!prefsHandler->readBlock("EnabledItems", (uint8_t *)&config->enabledStatusEntries, sizeof(config->enabledStatusEntries)))
    {
        memset((uint8_t *)&config->enabledStatusEntries, 0, sizeof(config->enabledStatusEntries));
    }
}

void StatusCSV::saveConfiguration() {
    if (!config) {
        config = new StatusCSVConfiguration();
        setConfiguration(config);
    }

    prefsHandler->write("TicksPer", config->ticksPerUpdate);
    if (!prefsHandler->writeBlock("EnabledItems", (uint8_t *)&config->enabledStatusEntries, sizeof(config->enabledStatusEntries)))
    {
        Logger::error("Could not write enabled status fields register!");
    }

    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}

DMAMEM StatusCSV statuscsv;
