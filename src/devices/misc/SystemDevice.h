/*
 * SystemDevice.h - Packages the system settings into a device so that
                    it conforms to the way everything else works 
                    (in terms of how settings are stored, etc).
 *
 Copyright (c) 2016 Collin Kidder, Michael Neuweiler, Charles Galpin

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

#ifndef SYSDEV_H_
#define SYSDEV_H_

#include <Arduino.h>
#include "../../config.h"
#include "../../Logger.h"
#include "../../DeviceManager.h"

class SystemConfiguration: public DeviceConfiguration {
public:
    uint8_t systemType;
    uint16_t adcGain[NUM_ANALOG];
    uint16_t adcOffset[NUM_ANALOG];
    uint32_t canSpeed[4];
    uint8_t swcanMode; //should can0 be in SWCAN mode?
    uint8_t logLevel;
};

class SystemDevice: public Device {
public:
    SystemDevice();
    void setup();
    void earlyInit();
    void handleTick();
    DeviceId getId();
    DeviceType getType();

    void loadConfiguration();
    void saveConfiguration();

protected:

private:
};

extern SystemConfiguration *sysConfig;

#endif
