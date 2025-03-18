/*
 * LightController.h - Handles various lighting aspects that are probably required. Can trigger the reverse light
                       and the brake light based upon the state of the motor controller and the pedal and brake inputs
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

#ifndef LIGHTCTRL_H_
#define LIGHTCTRL_H_

#include <Arduino.h>
#include "../../config.h"
#include "../io/Throttle.h"
#include "../motorctrl/MotorController.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../Logger.h"
#include "../../DeviceManager.h"
#include "../../FaultHandler.h"

#define LIGHTCTRL 0x3300
#define CFG_TICK_INTERVAL_LIGHTING     40000

class LightingConfiguration: public DeviceConfiguration {
public:
    uint8_t brakeLightOutput;
    uint8_t reverseLightOutput;
    float reqRegenLevel; //how much negative throttle request is needed to turn on brake light
};

class LightController: public Device {
public:
    LightController();
    void setup();
    void handleTick();

    void loadConfiguration();
    void saveConfiguration();

protected:

private:
    LightingConfiguration *config;
};

#endif
