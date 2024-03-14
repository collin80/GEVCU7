/*
 * CKMotorController.h
 *
 * Created: 7/7/2016 2:47:31 PM
 *  Author: collin
 
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


#ifndef CKMOTORCONTROLLER_H_
#define CKMOTORCONTROLLER_H_

#include <Arduino.h>
#include "../../config.h"
#include "MotorController.h"
#include "../../sys_io.h"
#include "../../TickHandler.h"
#include "../../CanHandler.h"

#define CKINVERTER 0x1003
#define CFG_TICK_INTERVAL_MOTOR_CONTROLLER_CK     40000

/*
 * Class for controller specific configuration parameters
 */
class CKMotorControllerConfiguration : public MotorControllerConfiguration {
public:
};

class CKMotorController: public MotorController, CanObserver {
public:

public:
    virtual void handleTick();
    virtual void handleCanFrame(const CAN_message_t &frame);
    virtual void setup();
    void earlyInit();
    void setGear(Gears gear);

    CKMotorController();
    DeviceId getId();
    uint32_t getTickInterval();

    virtual void loadConfiguration();
    virtual void saveConfiguration();

private:

    OperationState actualState; //what the controller is reporting it is    
	uint8_t aliveCounter;
    void timestamp();
	byte calcChecksum(CAN_message_t& thisFrame);
    void sendPowerCmd();
    //byte calcChecksum(CAN_FRAME thisFrame);

};

#endif /* CKMOTORCONTROLLER_H_ */