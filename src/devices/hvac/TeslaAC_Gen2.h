/*
 * TeslaAC_Gen2.h - Implements control and status reading of Tesla Model S Gen2
 * air conditioning compressor over CAN bus.
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
#include "../../CanHandler.h"
#include "HVACDevice.h"
#include "../../PID_v1.h"

#define TESLA_AC_GEN2 0x4212
#define CFG_TICK_INTERVAL_COMPRESSOR 100000

/*
 * Class for Delphi DCDC specific configuration parameters
 */
class TeslaACGen2Configuration : public HVACConfiguration {
public:
    uint8_t canbusNum;
    uint16_t maxPower;
    double kP, kI, kD;
};

class TeslaACGen2Controller: public HVACController, CanObserver {
public:
    virtual void handleTick();
    virtual void handleCanFrame(const CAN_message_t &frame);
    virtual void setup();

    TeslaACGen2Controller();
    void timestamp();
    uint32_t getTickInterval();

    virtual void loadConfiguration();
    virtual void saveConfiguration();

private:
    void sendCmd();
    bool isReady;
    PID *pid;
    double targetDuty;
    double targetTempC;
};


