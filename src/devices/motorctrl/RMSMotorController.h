/*
 * RMSMotorController.h
 *
 *
 Copyright (c) 2017 Collin Kidder

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

#ifndef RMSCTRL_H_
#define RMSCTRL_H_

#include <Arduino.h>
#include "../../config.h"
#include "MotorController.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../CanHandler.h"

/*
 * Class for Rinehart PM Motor Controller specific configuration parameters
 */
class RMSMotorControllerConfiguration : public MotorControllerConfiguration {
public:
};

class RMSMotorController: public MotorController, CanObserver {

public:
    virtual void handleTick();
    virtual void handleCanFrame(const CAN_message_t &frame);
    virtual void setup();
    void earlyInit();

    RMSMotorController();
    DeviceId getId();
    uint32_t getTickInterval();

    virtual void loadConfiguration();
    virtual void saveConfiguration();

private:
    byte online; //counter for whether we're getting traffic from RMS controller
    byte alive;
    int activityCount;
    byte sequence;
    uint16_t torqueCommand;
	uint32_t mss;
	bool isLockedOut;
	bool isEnabled;
	bool isCANControlled;

   void sendCmdFrame();
   void handleCANMsgTemperature1(uint8_t *data);
   void handleCANMsgTemperature2(uint8_t *data);
   void handleCANMsgTemperature3(uint8_t *data);
   void handleCANMsgAnalogInputs(uint8_t *data);
   void handleCANMsgDigitalInputs(uint8_t *data);
   void handleCANMsgMotorPos(uint8_t *data);
   void handleCANMsgCurrent(uint8_t *data);
   void handleCANMsgVoltage(uint8_t *data);
   void handleCANMsgFlux(uint8_t *data);
   void handleCANMsgIntVolt(uint8_t *data);
   void handleCANMsgIntState(uint8_t *data);
   void handleCANMsgFaults(uint8_t *data);
   void handleCANMsgTorqueTimer(uint8_t *data);
   void handleCANMsgModFluxWeaken(uint8_t *data);
   void handleCANMsgFirmwareInfo(uint8_t *data);
   void handleCANMsgDiagnostic(uint8_t *data);
};

#endif /* RMSCTRL_H_ */



