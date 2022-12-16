#pragma once

/*
 * LeafMotorController.h
 *
 * Heavy amounts of peeking were done into the STM32-VCU code
 * found here: https://github.com/damienmaguire/Stm32-vcu/blob/master/src/leafinv.cpp
 * All credit for the actual implementation details goes to Damien, et al.
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

#include <Arduino.h>
#include "../../config.h"
#include "MotorController.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../CanHandler.h"

#define LEAFINV 0x100A
#define CFG_TICK_INTERVAL_MOTOR_CONTROLLER_LEAF     40000

/*
 * Class for DMOC specific configuration parameters
 */
class LeafMotorControllerConfiguration : public MotorControllerConfiguration {
public:
    uint8_t canbusNum;
};

class LeafMotorController: public MotorController, CanObserver {
public:
    virtual void handleTick();
    virtual void handleCanFrame(const CAN_message_t &frame);
    virtual void setup();
    void earlyInit();
    void setGear(Gears gear);

    LeafMotorController();
    DeviceId getId();
    uint32_t getTickInterval();
    void taperRegen();

    virtual void loadConfiguration();
    virtual void saveConfiguration();

private:

    OperationState actualState; //what the controller is reporting it is
    int step;
    byte online; //counter for whether Leaf inverter appears to be operating
    byte alive;
    int activityCount;
    uint16_t torqueCommand;
    long ms;
    void sendFrame11A(); //gear selection, car on/off
    void sendFrame1D4(); //torque request, charge status
    void sendFrame1DB(); //battery status msg 1
    void sendFrame1DC(); //battery status msg 2
    void sendFrame1F2(); //charger cmd from vcu
    void sendFrame50B(); //vcu sends, hcm wake up. Do we need it?
    void sendFrame55B(); //battery msg, SOC, misc stuff
    void sendFrame59E(); //fast charge stuff for bms
    void sendFrame5BC(); //bms - remaining charge, output limit reason, temperature to dash

    byte calcChecksum(CAN_message_t &thisFrame);

};
