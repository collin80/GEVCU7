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
typedef void (Device::*AfterUpdate)();
#define DEV_PTR(p) static_cast<DescribeValue>(p)
#define UPD_PTR(p) static_cast<AfterUpdate>(p)

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
    DEVICE_HVAC,
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

//Definition of the array is in DeviceManager.cpp
extern const char *CFG_VAR_TYPE_NAMES[7];

//all of these should be 64 bit on this hardware
union minMaxType
{
    uint64_t u_int;
    int64_t s_int;
    double floating;
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
    minMaxType minValue; //minimum acceptable value
    minMaxType maxValue; //maximum acceptable value
    uint8_t precision; //number of decimal places to display. Obv. 0 for integers
    DescribeValue descFunc; //if this function pointer is non-null it'll be used to turn values into strings.
    AfterUpdate afterUpdateFunc; //called after this value is updated 
};

/*StatusEntry records a data item that the device would like to expose to the world. This is different
from ConfigEntry in that you cannot edit these values. Instead these values are only updated by the device
itself. The format here is a little bit weird but for reasons of making life easier on device developers.
Since there is a pointer to the actual variable the device developer doesn't need to worry about updating
these. All the developer needs to do is register these entries. The Device Manager class now is also
managing the status updates. Every tick the device manager will look through all status entries and see
if the value has changed for any of them. If so, it dispatches a message to any status watchers.
*/
struct StatusEntry
{
    String statusName;
    void *varPtr;
    CFG_ENTRY_VAR_TYPE varType;
    double lastValue;
    Device *device;
    uint32_t hash;

    StatusEntry()
    {
        statusName = String();
        varPtr = nullptr;
        varType = CFG_ENTRY_VAR_TYPE::BYTE;
        lastValue = 0.0;
        device = nullptr;
        hash = 0;
    }

    StatusEntry(String name, void *ptr, CFG_ENTRY_VAR_TYPE type, double val, Device *dev)
    {
        statusName = name;
        varPtr = ptr;
        varType = type;
        lastValue = val;
        device = dev;
        hash = fnvHash(statusName.c_str());
    }

    static uint32_t fnvHash(const char *input)
    {
        uint32_t hash = 2166136261ul;
        char c;
        while (*input)
        {
            c = *input++;
            c = toupper(c); //force all names to uppercase just to make it consistent
            hash = hash ^ c;
            hash = hash * 16777619;
        }
        return hash;
    }

    uint32_t getHash()
    {
        if (!hash) hash = fnvHash(statusName.c_str());
        return hash;
    }

    //creates the prettiest version of the value it can. It will interpret integers as integers, print floats
    //with only a few decimal places, etc.
    String getValueAsString()
    {
        String out;
        if (varType == BYTE)
        {
            uint8_t v = *((uint8_t*)varPtr);
            out = String(v);
            return out;
        }
        if (varType == STRING)
        {
            char *str = (char *)varPtr;
            //while (str) out += *str++;
            out = str;
            return out;
        }
        if (varType == INT16)
        {
            int16_t v = *((int16_t*)varPtr);
            out = String(v);
            return out;
        }
        if (varType == UINT16)
        {
            uint16_t v = *((uint16_t*)varPtr);
            out = String(v);
            return out;
        }
        if (varType == INT32)
        {
            int32_t v = *((int32_t*)varPtr);
            out = String(v);
            return out;
        }
        if (varType == UINT32)
        {
            uint32_t v = *((uint32_t*)varPtr);
            out = String(v);
            return out;
        }
        if (varType == FLOAT)
        {
            float v = *((float*)varPtr);
            out = String(v, 3);
            return out;
        }
        out = "";
        return out;
    }

    double getValueAsDouble()
    {
        double out;
        if (varType == BYTE)
        {
            uint8_t v = *((uint8_t*)varPtr);
            out = (double)v;
            return out;
        }
        if (varType == STRING) //this one is special. Sum all characters to give a numeric result
        {
            char *str = (char *)varPtr;
            while (str) out += *str++;
            return out;
        }
        if (varType == INT16)
        {
            int16_t v = *((int16_t*)varPtr);
            out = (double)v;
            return out;
        }
        if (varType == UINT16)
        {
            uint16_t v = *((uint16_t*)varPtr);
            out = (double)v;
            return out;
        }
        if (varType == INT32)
        {
            int32_t v = *((int32_t*)varPtr);
            out = (double)v;
            return out;
        }
        if (varType == UINT32)
        {
            uint32_t v = *((uint32_t*)varPtr);
            out = (double)v;
            return out;
        }
        if (varType == FLOAT)
        {
            float v = *((float*)varPtr);
            out = (double)v;
            return out;
        }
        out = 0.0;
        return out;
    }
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
#define SYSIO  0x7400
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


