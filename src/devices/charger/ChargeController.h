/*
 * ChargeController.h
 *
 *
 *
 Copyright (c) 2022 Collin Kidder

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
#include "../../CanHandler.h"

enum CHARGER_FAULTS
{
    CHARGER_FAULT_INPUTV = 1000,
    CHARGER_FAULT_INPUTA,
    CHARGER_FAULT_OUTPUTV,
    CHARGER_FAULT_OUTPUTA,
    CHARGER_LAST_FAULT
};

extern const char* CHARGER_FAULT_DESCS[];

/*
 * Class for HV charger specific configuration parameters
 */
class ChargeConfiguration : public DeviceConfiguration {
public:
    float targetUpperVoltage;
    float targetCurrentLimit;
    uint8_t requireHVReady; //if set only enable charger if HV seems ready. Would have to query BMS or Precharge device for that.
    uint8_t enablePin; //if charger requires an enable pin then it can be set here
};

class ChargeController: public Device {
public:
    virtual void handleTick();
    virtual void setup();

    ChargeController();
    float getOutputVoltage();
    float getOutputCurrent();
    float getTemperature();
    bool getEVSEConnected();
    virtual const char* getFaultDescription(uint16_t faultcode);

    virtual void loadConfiguration();
    virtual void saveConfiguration();

protected:
    float outputVoltage;
    float outputCurrent;
    float deviceTemperature;
    bool isEnabled;
    bool isFaulted;
    bool isEVSEConnected;
};
