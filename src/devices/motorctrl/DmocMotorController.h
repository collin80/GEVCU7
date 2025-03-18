/*
 * DmocMotorController.h
 *
 * Note that the dmoc needs to have some form of input for gear selector (drive/neutral/reverse)
 *
 Copyright (c) 2013 Collin Kidder, Michael Neuweiler, Charles Galpin

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

#ifndef DMOC_H_
#define DMOC_H_

#include <Arduino.h>
#include "../../config.h"
#include "MotorController.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../CanHandler.h"

#define DMOC645 0x1000
#define CFG_TICK_INTERVAL_MOTOR_CONTROLLER_DMOC     40000

/*
 * Class for DMOC specific configuration parameters
 */
class DmocMotorControllerConfiguration : public MotorControllerConfiguration {
public:
    uint8_t canbusNum;
};

class DmocMotorController: public MotorController, CanObserver {
public:

    enum Step {
        SPEED_TORQUE,
        CHAL_RESP
    };

    enum KeyState {
        OFF = 0,
        ON = 1,
        RESERVED = 2,
        NOACTION = 3
    };



public:
    virtual void handleTick();
    virtual void handleCanFrame(const CAN_message_t &frame);
    virtual void setup();
    void setGear(Gears gear);

    DmocMotorController();
    uint32_t getTickInterval();
    void taperRegen();

    virtual void loadConfiguration();
    virtual void saveConfiguration();

private:

    OperationState actualState; //what the controller is reporting it is
    int step;
    byte alive;
    uint16_t torqueCommand;
    uint8_t inhibitStateMachine;
    DmocMotorControllerConfiguration *config;

    void timestamp();
    void sendCmd1();
    void sendCmd2();
    void sendCmd3();
    void sendCmd4();
    void sendCmd5();
    byte calcChecksum(const CAN_message_t &thisFrame);
};

#endif /* DMOC_H_ */



