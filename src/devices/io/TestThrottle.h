/*
 * TestThrottle.h - A throttle that runs itself up and down for testing purposes. No analog inputs. It just does its thing
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

#ifndef TEST_POT_H_
#define TEST_POT_H_

#include <Arduino.h>
#include "../../config.h"
#include "Throttle.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../Logger.h"
#include "../../DeviceManager.h"
#include "../../FaultHandler.h"

#define TESTACCEL 0x104F
#define CFG_TICK_INTERVAL_POT_THROTTLE              40000

/*
 * The extended configuration class with additional parameters for PotThrottle
 */
class TestThrottleConfiguration: public ThrottleConfiguration {
public:
    //still used to simulate an incoming ADC that is producing values
    uint16_t minimumLevel1, maximumLevel1;
};

class TestThrottle: public Throttle {
public:
    TestThrottle();
    void setup();
    void handleTick();
    RawSignalData *acquireRawSignal();

    void loadConfiguration();
    void saveConfiguration();

protected:
    bool validateSignal(RawSignalData *);
    int16_t calculatePedalPosition(RawSignalData *);

private:
    RawSignalData rawSignal;
    bool rampingDirection;
};

#endif /* POT_THROTTLE_H_ */


