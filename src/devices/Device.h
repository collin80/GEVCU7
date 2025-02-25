/*
 * Device.h
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

#ifndef DEVICE_H_
#define DEVICE_H_

#include <Arduino.h>
#include <vector>
#include "../config.h"
#include "DeviceTypes.h"
#include "../eeprom_layout.h"
#include "../PrefHandler.h"
#include "../Sys_Messages.h"
#include "../FaultHandler.h"
#include "../CrashHandler.h"

/*
 * A abstract class to hold device configuration. It is to be accessed
 * by sub-classes via getConfiguration() and then cast into its
 * correct sub-class.
 */
class DeviceConfiguration {

};

//generic device faults start at 0
enum DEVICEFAULTS
{
    NO_FAULT = 0,
    CAN_COMM_FAULT,
    COMM_TIMEOUT,
    DEVICE_NOT_ENABLED,
    DEVICE_OVER_TEMP,
    DEVICE_UNDER_TEMP,
    DEVICE_OVERV,
    DEVICE_UNDERV,
    DEVICE_HARDWARE_FAULT,
    GENERAL_FAULT,

    LAST_FAULT_CODE
};

static const char* DEVICE_FAULT_DESCS[] =
{
    "No fault",
    "CAN communications fault",
    "Communications timeout",
    "Device is not enabled",
    "Over temperature limit",
    "Under temperature limit",
    "Over voltage",
    "Under voltage",
    "Hardware fault",
    "General fault"
};

/*
 * A abstract class for all Devices.
 */
class Device: public TickObserver {
public:
    Device();
    virtual void setup();
    virtual void earlyInit();
    virtual void handleMessage(uint32_t, const void* );
    virtual void disableDevice();
    void handleTick();
    bool isEnabled();
    DeviceId getId();
    DeviceType getType();
    virtual uint32_t getTickInterval();
    const char* getCommonName();
    const char* getShortName();
    void forceEnableState(bool state);

    virtual void loadConfiguration();
    virtual void saveConfiguration();
    DeviceConfiguration *getConfiguration();
    void zapConfiguration();
    void setConfiguration(DeviceConfiguration *);
    const std::vector<ConfigEntry> *getConfigEntries();
    const ConfigEntry* findConfigEntry(const char *settingName);
    virtual const char* getFaultDescription(uint16_t faultcode);

protected:
    PrefHandler *prefsHandler;
    const char *commonName;
    const char *shortName;
    DeviceId deviceId;
    DeviceType deviceType;
    std::vector<ConfigEntry> cfgEntries;

private:
    DeviceConfiguration *deviceConfiguration; // reference to the currently active configuration
};

#endif /* DEVICE_H_ */


