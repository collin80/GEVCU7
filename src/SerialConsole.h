/*
 * SerialConsole.h
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

#ifndef SERIALCONSOLE_H_
#define SERIALCONSOLE_H_

#include <vector>
#include "config.h"
#include "Heartbeat.h"
#include "MemCache.h"
#include "SdFat.h"
#include "eeprom_layout.h"
#include "sys_io.h"
#include "devices/io/PotThrottle.h"
#include "DeviceManager.h"
#include "devices/motorctrl/MotorController.h"
#include "devices/motorctrl/DmocMotorController.h" //TODO: direct reference to dmoc must be removed
#include "devices/io/ThrottleDetector.h"
#include "devices/bms/BatteryManager.h"
#include "devices/misc/Precharger.h"

#define CONSOLE_QUEUE_SIZE 4 //# of complete command lines that can be waiting to run

class SerialConsole {
public:
    SerialConsole(MemCache* memCache);
    SerialConsole(MemCache* memCache, Heartbeat* heartbeat);
    void injectChar(char c);
    void loop();
    void runQueuedCommands();
    void setup();
    void printMenu();

protected:
    enum CONSOLE_STATE
    {
        STATE_ROOT_MENU
    };

private:
    MemCache* memCache;
    Heartbeat* heartbeat;
    bool handlingEvent;
    char cmdBuffer[80]; //the line currently being executed. Only touched by runQueuedCommands (thread 0)
    int ptrBuffer;
    //USB serial and the ESP32 (telnet) both feed the console from different threads, often from inside
    //yield() in whatever thread happened to call it, so each builds its line in its own buffer. Finished
    //lines are only queued there. runQueuedCommands() runs them later from loop() where no other locks are held.
    char usbBuffer[80];
    int usbPtr;
    char injectBuffer[80];
    int injectPtr;
    char lineQueue[CONSOLE_QUEUE_SIZE][80];
    int lineQueueLen[CONSOLE_QUEUE_SIZE];
    int queueHead; //next slot to fill
    int queueTail; //next slot to run
    int queueCount;
    RecursiveMutex consoleMutex; //guards the line queue only
    int state;
    int loopcount;
    bool cancel;
    FsFile file;

    void init();
    void serialEvent();
    void addInputChar(char c, char *lineBuf, int &linePtr);
    void submitLine(const char *line, int len);
    void handleConsoleCmd();
    void handleShortCmd();
    void handleConfigCmd();
    void resetWiReachMini();
    void getResponse();
    void printConfigEntry(const Device *dev, const ConfigEntry &entry);
    void getConfigEntriesForDevice(Device *dev);
    void updateSetting(const char *settingName, char *valu);
    void generateEEPROMBinary();
    void loadEEPROMBinary();
    void generateEEPROMJSON();
    void loadEEPROMJSON();
};

#endif /* SERIALCONSOLE_H_ */


