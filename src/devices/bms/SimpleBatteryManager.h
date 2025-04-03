/*
 * SimpleBatteryManager.h
 *
 * A simple BMS of sorts using just data the G7 has access to
 *
Copyright (c) 2025 Collin Kidder

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

#ifndef SIMPLEBATT_H_
#define SIMPLEBATT_H_

#include <Arduino.h>
#include "../../config.h"
#include "../Device.h"
#include "../../DeviceManager.h"
#include "BatteryManager.h"

#define SIMPLEBMS 0x2020
#define CFG_TICK_INTERVAL_BMS_SIMPLE                 40000

class SimpleBatteryManagerConfiguration : public BatteryManagerConfiguration
{
public:
    float nominalPackAH;
    float currentPackAH;
    float packEmptyVoltage;
    float packFullVoltage;
};

class SimpleBatteryManager : public BatteryManager
{
public:
    SimpleBatteryManager();
    void setup();
    void handleTick();
    bool hasPackVoltage();
    bool hasPackCurrent();
    bool hasTemperatures();
    bool hasLimits();
    bool isChargeOK();
    bool isDischargeOK();

    virtual void loadConfiguration();
    virtual void saveConfiguration();

protected:
private:
    SimpleBatteryManagerConfiguration *config;
    bool firstReading;
    uint32_t lastMS;
};

#endif


