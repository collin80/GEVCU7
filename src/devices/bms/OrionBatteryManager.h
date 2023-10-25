/*
 * OrionBatteryManager.h
 *
 * Read messages from OrionBMS
 *
Copyright (c) 2023 Collin Kidder, Michael Neuweiler, Charles Galpin

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

#ifndef ORIONBATT_H_
#define ORIONBATT_H_

#include <Arduino.h>
#include "../../config.h"
#include "../Device.h"
#include "../../DeviceManager.h"
#include "BatteryManager.h"
#include "../../CanHandler.h"

#define ORIONBMS 0x2010
#define CFG_TICK_INTERVAL_BMS_ORION                 500000

class OrionBatteryManagerConfiguration : public BatteryManagerConfiguration
{
public:
    uint8_t canbusNum;
};

class OrionBatteryManager : public BatteryManager, CanObserver
{
public:
    OrionBatteryManager();
    void setup();
    void earlyInit();
    void handleTick();
    void handleCanFrame(const CAN_message_t &frame);
    DeviceId getId();
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
    void sendKeepAlive();
};

#endif


