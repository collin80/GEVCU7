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

#ifndef DEVICE_TYPES_H_
#define DEVICE_TYPES_H_

#include <Arduino.h>

class Device;
typedef String (Device::*DescribeValue)();
#define DEV_PTR(p) static_cast<DescribeValue>(p)

enum DeviceType {
    DEVICE_ANY,
    DEVICE_MOTORCTRL,
    DEVICE_BMS,
    DEVICE_CHARGER,
    DEVICE_DISPLAY,
    DEVICE_THROTTLE,
    DEVICE_BRAKE,
    DEVICE_MISC,
    DEVICE_WIFI,
    DEVICE_IO,
    DEVICE_DCDC,
    DEVICE_NONE
};

enum CFG_ENTRY_VAR_TYPE
{
    BYTE,
    STRING,
    INT16,
    UINT16,
    INT32,
    UINT32,
    FLOAT
};

/*
ConfigEntry records store configuration options that a given device would like to expose to the world
so that they can be edited either on the serial console or the webpage (or any other way, CAN, etc).
Includes basically all the stuff that SerialConsole used to have hard coded.
There are hierachical calls to parent classes to find all configuration items. So, classes can
register config entries but also subclasses can have their own on top of the base class, etc.
*/
struct ConfigEntry
{
    String cfgName; //the short name we'll use to set this on the serial console. AKA: CFGNAME=SomeValue
    String helpText; //also shown on serial console to explain the configuration option
    void *varPtr; //pointer to the variable whose value we'd like to get or set
    CFG_ENTRY_VAR_TYPE varType; //what sort of variable were we pointing to?
    float minValue; //minimum acceptable value
    float maxValue; //maximum acceptable value
    uint8_t precision; //number of decimal places to display. Obv. 0 for integers
    DescribeValue descFunc; //if this function pointer is non-null it'll be used to turn values into strings.
};

/*
you used to find a giant ENUM of all the device ids here. It isn't here now because
now each device tracks its own ID and so the ID for each device is in that device's
header files. This does away with this convenient list but means that device driver
developers don't need to update this list when they add a device. Just don't use the same
id as anything else. Yes, that's more difficult now but you can ask the system for a list
of all registered devices and get the list that way.
*/
//    BRUSACHARGE = 0x1010,
//    TCCHCHARGE = 0x1011,
//    LEAR=0x1012,

//but some devices that are internal to the system do need to be listed here.
//System devices reserve IDs 0x7000 through 0x7FFF which is a lot of reserved
//IDs but you also aren't going to be registering thousands of devices either
//IDs 0x8000 and up are off limits as the high bit is used for enable/disable
//but this still leaves many IDs possible.
#define FAULTSYS 0x7000
#define SYSTEM 0x7100
#define HEARTBEAT 0x7200
#define MEMCACHE 0x7300
#define INVALID 0xFFFF

typedef uint16_t DeviceId;

namespace LatchModes
{
    enum LATCHMODE
    {
        NO_LATCHING, //always reads the actual current input state
        LATCHING, //input state sticks "ON" until it is actually read then is automatically cleared
        TOGGLING, //Pushing a button toggles it on/off
        LOCKING //input locks as ON until it is explicitly set off by request
    };
}

#endif /* DEVICE_TYPES_H_ */


