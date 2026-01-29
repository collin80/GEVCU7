/*
 * PowerController.h - Used to control the power state of the GEVCU7 controller. With this you can wake up
 and go to sleep based on either a pin or a CAN message. When transitioning power state, it notifies
 other devices so they can act accordingly. When sleeping we disable sending of CAN messages but potentially
 could receive them. Option to enable an output when awake. So, controller can control a contactor for switched
 12V power. Thus, with this module the ign switch could go to a digital input and the 12V is controlled via
 relay/contactor with this device
 *
 Copyright (c) 2025 Collin Kidder

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

#ifndef POWERCTRL_H_
#define POWERCTRL_H_

#include <Arduino.h>
#include "../../config.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../Logger.h"
#include "../../DeviceManager.h"
#include "../../FaultHandler.h"

#define POWERCTRL 0x3400
#define CFG_TICK_INTERVAL_POWER     100000

class PowerConfiguration: public DeviceConfiguration {
public:
    uint8_t powerTriggerPin;
    uint32_t powerTriggerCANID;
    uint8_t powerTriggerCANBit;
    uint8_t powerOutputPin;
    uint8_t canbusNum;
};

class PowerController: public Device, CanObserver {
public:
    PowerController();
    void setup();
    void handleTick();
    virtual void handleCanFrame(const CAN_message_t &frame);

    void loadConfiguration();
    void saveConfiguration();

protected:

private:
    PowerConfiguration *config;
    bool systemAwake;
    int countdown;
    int canTimer;

    void wakeup();
    void snooze();
};

#endif
