/*
 * AxiomaticWOC.h
 *
 * * 
 *
 *
 * Created: 2023
  *  Author: Collin Kidder

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

#ifndef AXIOWOC_H_
#define AXIOWOC_H_

#include <Arduino.h>
#include "../../config.h"
#include "../../constants.h"
#include "../Device.h"
#include "../../TickHandler.h"
#include "../../CanHandler.h"
#include "../../DeviceManager.h"
#include "../../Sys_Messages.h"
#include "../DeviceTypes.h"

#define AXIOWOC                     0x4700
#define CFG_TICK_INTERVAL_AXIOWOC   100000

class AxiomaticWOCConfiguration: public DeviceConfiguration {
public:
    uint8_t canbusNum;
};


class AxiomaticWOC: public Device, CanObserver {
public:

    AxiomaticWOC();
    virtual void handleTick();
    
    virtual void handleCanFrame(const CAN_message_t &frame);

    virtual void setup(); //initialization on start up

    void loadConfiguration();
    void saveConfiguration();

private:
    void sendLEDCmd();
    void sendWakeCfg();
};

#endif

