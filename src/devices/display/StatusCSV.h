/*
 * StatusCSV.h
 *
 * Outputs CSV formatted lines to the second serial port at a configurable rate
 *
 *
 * Created: Oct 2023
  *  Author: Collin Kidder

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

#ifndef STATUSCSV_H_
#define STATUSCSV_H_

#include <Arduino.h>
#include "SD.h"
#include "../../config.h"
#include "../../constants.h"
#include "../Device.h"
#include "../../TickHandler.h"
#include "../../DeviceManager.h"
#include "../../Sys_Messages.h"
#include "../DeviceTypes.h"

#define STATUSCSV                   0x4500
#define CFG_TICK_INTERVAL_STATUS    20000
#define NUM_ENTRIES_IN_TABLE        40

class StatusCSVConfiguration: public DeviceConfiguration {
public:
    uint16_t ticksPerUpdate;
    uint32_t enabledStatusEntries[NUM_ENTRIES_IN_TABLE]; //4 bytes per entry
    uint8_t bAutoStart;
    uint8_t bFileOutput;
    char enableString[100];
    char disableString[100];
};

class StatusCSV: public Device {
public:

    StatusCSV();
    virtual void handleTick();

    virtual void setup(); //initialization on start up
    void earlyInit();
    DeviceType getType();
    DeviceId getId();
    bool isHashMonitored(uint32_t hash);

    void loadConfiguration();
    void saveConfiguration();

private:
    StatusCSVConfiguration *config;
    uint32_t tickCounter;
    bool haveEnabledEntries;
    bool isEnabled;
    bool fileInitialized;
    bool needHeader;
    FsFile logFile;
    uint32_t lastWriteTime;

    void handleSerialSwitch();
    void enableStatusHash(char * str);
    void disableStatusHash(char * str);
    void initializeFile();
    void flushFile();
};

#endif

