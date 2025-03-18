/*
 * TeslaSDCDC.h
 *
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

#ifndef TSDCDC_H_
#define TSDCDC_H_

#include <Arduino.h>
#include "../../config.h"
#include "../Device.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../CanHandler.h"
#include "DCDCController.h"

#define TESLA_S_DCDC 0x1055
#define CFG_TICK_INTERVAL_DCDC                      200000

/*
 * Class for Tesla Model S DCDC specific configuration parameters
 */
class TSDCDCConfiguration : public DCDCConfiguration {
public:
    uint8_t canbusNum;
};

class TSDCDCController: public DCDCController, CanObserver {
public:
    virtual void handleTick();
    virtual void handleCanFrame(const CAN_message_t &frame);
    virtual void setup();

    TSDCDCController();
    uint32_t getTickInterval();

    virtual void loadConfiguration();
    virtual void saveConfiguration();

private:
    void sendCmd();
    TSDCDCConfiguration *config;
};

#endif /* TSDCDC_H_ */


