/*
 * HeatCoolController.h - implements a simple unified way to control fans, coolant pumps, heaters, etc that
 might be used in a car. 
 *
 Copyright (c) 2021 Collin Kidder, Michael Neuweiler, Charles Galpin

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

#ifndef HEATCOOL_H_
#define HEATCOOL_H_

#include <Arduino.h>
#include "../../config.h"
#include "../motorctrl/MotorController.h"
#include "../bms/BatteryManager.h"
#include "../dcdc/DCDCController.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../Logger.h"
#include "../../DeviceManager.h"
#include "../../FaultHandler.h"

#define HEATCOOL 0x3200
#define CFG_TICK_INTERVAL_HEATCOOL     200000
#define COOL_ZONES  3

//defines where a given zone gets its temperature readings from.
enum COOLZONE
{
    CZ_MOTORCTRL,
    CZ_BMS,
    CZ_DCDC
};


class HeatCoolConfiguration: public DeviceConfiguration {
public:
    float heatOnTemperature;
    float heatOffTemperature;
    float coolOnTemperature[COOL_ZONES];
    float coolOffTemperature[COOL_ZONES];
    COOLZONE coolZoneType[COOL_ZONES];
    uint8_t runPumpWithHeat;
    uint8_t runPumpAtSysReady;
    //I/O pins
    uint8_t heatEnablePin;
    uint8_t waterPumpPin;
    uint8_t coolPins[COOL_ZONES];
};

class HeatCoolController: public Device {
public:
    HeatCoolController();
    void setup();
    void handleTick();

    void loadConfiguration();
    void saveConfiguration();

protected:

private:
    bool isHeatOn;
    bool isCoolOn[COOL_ZONES];
    bool isPumpOn;
    HeatCoolConfiguration *config;
};

#endif
