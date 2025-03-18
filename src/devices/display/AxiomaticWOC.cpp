/*
 * AxiomaticWOC.cpp
 *
 * A driver for the discontinued Axiomatic Wake On Charge module. Why is it in the 
 * display drivers? Because we're basically only using it to control two LEDs. They
 * are not controlled really like they're outputs so this is not an IO class and does
 * not register itself with the system IO driver. The WOC module does send CAN back
 * but it may be of limited use. If you want the data, it's documented and ready to use.
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

#include "AxiomaticWOC.h"

/*
 * Constructor.
 */
AxiomaticWOC::AxiomaticWOC() : Device()
{
    commonName = "Axiomatic WOC";
    shortName = "AXIOWOC";
    deviceId = AXIOWOC;
    deviceType = DEVICE_DISPLAY;
}

/*
 * Initialization of hardware and parameters
 */
void AxiomaticWOC::setup() {

    Logger::info("add device: WinStar (id: %X, %X)", AXIOWOC, this);

    tickHandler.detach(this);//Turn off tickhandler

    loadConfiguration(); //Retrieve any persistant variables

    Device::setup(); // run the parent class version of this function

    ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type                Min Max Precision Funct
    entry = {"AXWOC-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr};
    cfgEntries.push_back(entry);

    setAttachedCANBus(config->canbusNum);

    attachedCANBus->attach(this, 0x622, 0x7FF, false);

    setAlive();
	
    tickHandler.attach(this, CFG_TICK_INTERVAL_AXIOWOC);
}

/*
Byte 0 is 4 status flags
    bits 0-1 = LED1 status (0 = off, 1 = on, 2=flashing)
    bits 2-3 = LED2 status
    bits 4-5 = wake output status
    bits 6-7 = control pilot status (might be useful for drive inhibit if a digital input isn't being used for that)
Byte 1 = control pilot duty cycle (0.5% increment)
Bytes 2-3 = elapsed time (seconds) Big endian
Bytes 4-5 = LV Battery voltage (mv)
*/
void AxiomaticWOC::handleCanFrame(const CAN_message_t &frame)
{
    float batt12V;
    uint16_t elapsedMinutes;
    uint8_t pilotDuty;
    uint8_t statusFlags;
    if (frame.id == 0x622)
    {
        setAlive();
        statusFlags = frame.buf[0];
        pilotDuty = frame.buf[1];
        elapsedMinutes = ((frame.buf[2] << 8) + frame.buf[3]);
        batt12V = ((frame.buf[4] << 8) + frame.buf[5]) / 1000.0;
    }
}

//This method handles periodic tick calls received from the tasker.
void AxiomaticWOC::handleTick()
{
    checkAlive(3000);
    sendLEDCmd();
}

void AxiomaticWOC::sendLEDCmd()
{
    CAN_message_t output;
    output.len = 8;
    output.id = 0x620;
    output.flags.extended = 0; //standard frame
    output.buf[0] = 5; //LED 1 and 2 control, each 2 bits. 00 = off, 01 = Solid, 10 = Flashing 11 = ?!?
    output.buf[1] = 0xA0; //LED1 brightness
    output.buf[2] = 100; //duty cycle in 1/2% increments
    output.buf[3] = 0x64; //length of cycle (50ms increments)
    output.buf[4] = 0x64; //LED2 brightness
    output.buf[5] = 0x64; //LED2 duty
    output.buf[6] = 0x64; //LED2 cycle length
    output.buf[7] = 0; //delay from LED1 to LED2 flash

    attachedCANBus->sendFrame(output);
}

//you do not need to send this command to get the WOC module to work.
void AxiomaticWOC::sendWakeCfg()
{
    CAN_message_t output;
    output.len = 8;
    output.id = 0x621;
    output.flags.extended = 0; //standard frame
    output.buf[0] = 0; //0-1 = wake cmd, 2-3=timer delay action 3-4=undervolt action. No idea what each action means
    output.buf[1] = 0; //timer delay setpoint in minutes (big end byte)
    output.buf[2] = 0; //timer delay little end byte (probably allows to delay charging for X minutes?)
    output.buf[3] = 0x2A; //undervolt setpoint in mv (big end byte)
    output.buf[4] = 0xF8; //undervolt setpoint low end byte (2AF8 = 11v)
    output.buf[5] = 0; //nothing
    output.buf[6] = 0; //nada
    output.buf[7] = 0; //zilch

    attachedCANBus->sendFrame(output);
}

void AxiomaticWOC::loadConfiguration()
{
    config = (AxiomaticWOCConfiguration *)getConfiguration();

    if (!config) {
        config = new AxiomaticWOCConfiguration();
        setConfiguration(config);
    }

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
}

void AxiomaticWOC::saveConfiguration()
{
    config = (AxiomaticWOCConfiguration *)getConfiguration();

    prefsHandler->write("CanbusNum", config->canbusNum);
    prefsHandler->forceCacheWrite();
}

DMAMEM AxiomaticWOC awoc;
