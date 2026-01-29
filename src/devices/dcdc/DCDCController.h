/*
 * DCDCController.h
 *
 *
 *
 Copyright (c) 2014 Jack Rickard

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

#ifndef DCDC_H_
#define DCDC_H_

#include <Arduino.h>
#include "../../config.h"
#include "../Device.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../CanHandler.h"

enum DCDC_FAULTS
{
    DCDC_FAULT_INPUTV = 1000,
    DCDC_FAULT_INPUTA,
    DCDC_FAULT_OUTPUTV,
    DCDC_FAULT_OUTPUTA,
    DCDC_LAST_FAULT
};

extern const char* DCDC_FAULT_DESCS[];

/*
 * Class for DCDC specific configuration parameters
 */
class DCDCConfiguration : public DeviceConfiguration {
public:
    float targetLowVoltage;
    uint8_t requireHVReady; //if set only enable DC/DC if HV seems ready. Would have to query BMS or Precharge device for that.
    uint8_t enablePin; //if DC/DC requires an enable pin then it can be set here
};

class DCDCController: public Device {
public:
    virtual void handleTick();
    virtual void setup();

    DCDCController();
    void timestamp();
    float getOutputVoltage();
    float getOutputCurrent();
    float getTemperature();
    virtual const char* getFaultDescription(uint16_t faultcode);

    virtual void loadConfiguration();
    virtual void saveConfiguration();

protected:
    float outputVoltage;
    float outputCurrent;
    float deviceTemperature;
    bool isEnabled;
    bool isFaulted;
};

#endif /* DCDC_H_ */


