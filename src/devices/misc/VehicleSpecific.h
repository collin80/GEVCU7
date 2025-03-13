/*
 * VehicleSpecific.h - All code specific to your particular vehicle. This keeps the rest of code generic.
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

#ifndef VEHICLE_SPECIFIC_H_
#define VEHICLE_SPECIFIC_H_

#include <Arduino.h>
#include "../../config.h"
#include "../io/Throttle.h"
#include "../motorctrl/MotorController.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../Logger.h"
#include "../../DeviceManager.h"
#include "../../FaultHandler.h"

#define VEHICLESPECIFIC 0x3000
#define CFG_TICK_INTERVAL_VEHICLE                   100000

class VehicleSpecific: public Device {
public:
    VehicleSpecific();
    void setup();
    void handleTick();

    void loadConfiguration();
    void saveConfiguration();

protected:

private:
    bool didInitialSetup;
    int waitTicksStartup;
};

#endif


