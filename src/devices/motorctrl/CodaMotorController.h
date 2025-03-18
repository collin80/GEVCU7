/*
 * CodaMotorController.h
 *
 * Note that the dmoc needs to have some form of input for gear selector (drive/neutral/reverse)
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

#ifndef CODA_H_
#define CODA_H_

#include <Arduino.h>
#include "../../config.h"
#include "MotorController.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../CanHandler.h"

#define CODAUQM 0x1002
#define CFG_TICK_INTERVAL_MOTOR_CONTROLLER_CODAUQM  10000

/*
 * Class for Coda UQM Powerphase 100 specific configuration parameters
 */
class CodaMotorControllerConfiguration : public MotorControllerConfiguration {
public:
};

class CodaMotorController: public MotorController, CanObserver {

public:
    virtual void handleTick();
    virtual void handleCanFrame(const CAN_message_t &frame);
    virtual void setup();

    CodaMotorController();
    void timestamp();
    uint32_t getTickInterval();

    virtual void loadConfiguration();
    virtual void saveConfiguration();

private:
    byte alive;
    byte sequence;
    uint16_t torqueCommand;
    CodaMotorControllerConfiguration *config;
    void sendCmd1();
    void sendCmd2();
    uint8_t genCodaCRC(uint8_t cmd, uint8_t torq_lsb, uint8_t torq_msb);
};

#endif /* CODA_H_ */



