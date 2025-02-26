/*
 * Precharger.h - Implements various precharging strategies in case the user does not have
                  a bms or other device which is handling this.
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

#ifndef PRECHARGE_H_
#define PRECHARGE_H_

#include <Arduino.h>
#include "../../config.h"
#include "../io/Throttle.h"
#include "../motorctrl/MotorController.h"
#include "../bms/BatteryManager.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../Logger.h"
#include "../../DeviceManager.h"
#include "../../FaultHandler.h"
#include "../../FaultCodes.h"

#define PRECHARGER 0x3100
#define CFG_TICK_INTERVAL_PRECHARGE     40000

enum PRECHARGE_STATE
{
    PRECHARGE_INIT,
    PRECHARGE_INPROGRESS,
    PRECHARGE_COMPLETE,
    PRECHARGE_FAULT
};

enum PRECHARGE_TYPE
{
    PT_TIME_DELAY=1,
    PT_WAIT_FOR_VOLTAGE=2
};

class PrechargeConfiguration: public DeviceConfiguration {
public:
    uint8_t prechargeType; //use PRECHARGE_TYPE enum above for values
    uint16_t prechargeTime; //in milliseconds. So, up to 65 seconds possible. If waiting for BMS/MC voltage instead this is max time to wait before faulting
    uint8_t prechargeRelay;
    uint8_t mainRelay;
    uint8_t enableInput;
};

class Precharger: public Device {
public:
    Precharger();
    void setup();
    void handleTick();

    void loadConfiguration();
    void saveConfiguration();

protected:

private:
    PRECHARGE_STATE state;
    uint32_t prechargeBeginTime;
    uint16_t targetVoltage;
    bool isPrecharged;
};

#endif
