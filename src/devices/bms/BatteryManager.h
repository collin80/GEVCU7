/*
 * BatteryManager.h
 *
 * Parent class for battery management / monitoring systems
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

#ifndef BATTMANAGE_H_
#define BATTMANAGE_H_

#include <Arduino.h>
#include "../../config.h"
#include "../Device.h"
#include "../../DeviceManager.h"

enum BMS_FAULTS
{
    BMS_FAULT_CELL_UNDERV = 1000,
    BMS_FAULT_CELL_OVERV,
    BMS_FAULT_CELL_OVERT,
    BMS_FAULT_CELL_UNDERT,
    BMS_FAULT_CELL_INBALANCE,
    BMS_FAULT_BALANCING,
    BMS_FAULT_CURR_SENSING,
    BMS_FAULT_CURR_TOOPOS,
    BMS_FAULT_CURR_TOONEG,
    BMS_FAULT_CONTACTORA_STUCK_OPEN,
    BMS_FAULT_CONTACTORA_STUCK_CLOSED,
    BMS_FAULT_CONTACTORB_STUCK_OPEN,
    BMS_FAULT_CONTACTORB_STUCK_CLOSED,
    BMS_FAULT_PRECHARGE_STUCK_OPEN,
    BMS_FAULT_PRECHARGE_STUCK_CLOSED,
    BMS_FAULT_PRECHARGE_FAILURE,
    BMS_LAST_FAULT
};

static const char* BMS_FAULT_DESCS[] =
{
    "One or more cells under min voltage",
    "One or more cells over max voltage",
    "One or more cells over max temperature",
    "One or more cells under min temperature",
    "Max to min cell variation too high",
    "Fault in cell balancing",
    "Fault in current sensing",
    "Positive current is too large!",
    "Negative ccurrent is too large!"
    "Contactor A stuck open",
    "Contactor A stuck closed",
    "Contactor B stuck open",
    "Contactor B stuck closed",
    "Precharge contactor stuck open",
    "Precharge contactor stuck closed",
    "Precharge process has failed",
};

class BatteryManagerConfiguration : public DeviceConfiguration {
public:
};

class BatteryManager : public Device {
public:
    BatteryManager();
    ~BatteryManager();
    float getPackVoltage();
    float getPackCurrent();
    float getSOC();
    //bool allowCharging();
    //bool allowDischarging();
    float getHighestTemperature();
    float getLowestTemperature();
    float getChargeLimit();
    float getDischargeLimit();
    void setup();
    void handleTick();
    
    void loadConfiguration();
    void saveConfiguration();
    virtual const char* getFaultDescription(uint16_t faultcode);
    
    //a bunch of boolean functions. Derived classes must implment
    //these functions to tell everyone else what they support
    virtual bool hasPackVoltage() = 0;
    virtual bool hasPackCurrent() = 0;
    virtual bool hasTemperatures() = 0;
    virtual bool hasLimits() = 0;
    virtual bool isChargeOK() = 0;
    virtual bool isDischargeOK() = 0;
protected:
    float packVoltage;
    float packCurrent;
    float SOC; //state of charge in percent
    float lowestCellV, highestCellV;
    float lowestCellTemp, highestCellTemp;
    //should be some form of discharge and charge limit. I don't know if it should be % or amps
    //some BMS systems will report one way and some the other.
    float dischargeLimit, chargeLimit;
    bool allowCharge, allowDischarge;

private:
};

#endif


