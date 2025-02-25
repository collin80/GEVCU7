/*
 * TestMotorController.h
 *
 * Pretends to be an inverter that's set to go in drive gear. Returns reasonable
 * values for its current speed and torque based on accelerator pedal position.
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

#ifndef _TESTINVERTER_H_
#define _TESTINVERTER_H_

#include <Arduino.h>
#include "../../config.h"
#include "MotorController.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../CanHandler.h"

#define TESTINVERTER 0x100F
#define CFG_TICK_INTERVAL_MOTOR_CONTROLLER_TEST     40000

/*
 * Class for DMOC specific configuration parameters
 */
class TestMotorControllerConfiguration : public MotorControllerConfiguration {
public:
};

class TestMotorController: public MotorController {
public:

public:
    virtual void handleTick();
    virtual void setup();
    void earlyInit();
    void setGear(Gears gear);

    TestMotorController();
    uint32_t getTickInterval();

    virtual void loadConfiguration();
    virtual void saveConfiguration();

private:

    uint16_t torqueCommand;
    void timestamp();
};

#endif /* DMOC_H_ */



