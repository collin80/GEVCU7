/*
 * HVACDevice.h - Parent class for climate control devices (Air conditioning, cabin heat)
 * There was already HeatCoolController in the misc directory. That version is for
 * non-climate control heating and cooling - battery packs, inverters, etc. It's more for
 * turning on fans and pumps when things get hot or preheating a battery when it is too cold.
 * This one is for in the cabin climate control systems instead.
 *
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

#pragma once

#include <Arduino.h>
#include "../../config.h"
#include "../Device.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"

/*
 * Class for HVAC device common configuration
 */
class HVACConfiguration : public DeviceConfiguration {
public:
    float targetTemperature;
};

class HVACController: public Device {
public:
    virtual void handleTick();
    virtual void setup();

    HVACController();
    double getTemperature();
    double getWattage();

    virtual void loadConfiguration();
    virtual void saveConfiguration();

protected:
    bool canHeat;
    bool canCool;
    bool isFaulted;
    double currentTemperature;
    double wattage;
};


