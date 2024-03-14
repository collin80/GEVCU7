/*
 * C300MotorController.h
 *
 *
 Copyright (c) 2021 EVTV

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

#ifndef C300_H_
#define C300_H_

#include <Arduino.h>
#include "../../config.h"
#include "MotorController.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../CanHandler.h"

#define C300INV 0x1005
#define CFG_TICK_INTERVAL_MOTOR_CONTROLLER_C300     20000

/*
 * Class for C300 specific configuration parameters
 */
class C300MotorControllerConfiguration : public MotorControllerConfiguration {
public:
};

class C300MotorController: public MotorController, CanObserver {
public:
    virtual void handleTick();
    virtual void handleCanFrame(const CAN_message_t &frame);
    virtual void setup();
    void earlyInit();

    C300MotorController();
    DeviceId getId();
    uint32_t getTickInterval();
    void taperRegen();

    virtual void loadConfiguration();
    virtual void saveConfiguration();

private:

    OperationState actualState; //what the controller is reporting it is
    uint16_t torqueCommand;
    uint16_t maxAllowedTorque;
    uint32_t ms;
    int alive;
    bool prechargeComplete;
    bool allowedToOperate;
    void timestamp();

    void sendCmdUS();
    void sendCmdCanada();
};

#endif /* C300_H_ */



