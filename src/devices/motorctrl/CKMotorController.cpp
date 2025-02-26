/*
 * CKMotorController.cpp
 *
 * Created: 7/7/2016 2:47:16 PM
 *  Author: collin
 
  * Interface to the CK Motor Controller - Handles sending of commands and reception of status frames
  *
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

#include "CKMotorController.h"

uint32_t CK_milli;

CKMotorController::CKMotorController() : MotorController() {    
    operationState = DISABLED;
    actualState = DISABLED;
	aliveCounter = 0;
    commonName = "CK Inverter Ctrl Board";
    shortName = "CKInverter";
    deviceId = CKINVERTER;
}

void CKMotorController::setup() 
{
    tickHandler.detach(this);

    Logger::info("add device: CKINVCTRL (id:%X, %X)", CKINVERTER, this);

    loadConfiguration();
    MotorController::setup(); // run the parent class version of this function

    // register ourselves as observer of 0x23x and 0x65x can frames
    canHandlerIsolated.attach(this, 0x410, 0x7f0, false);

    running = false;
    //setSelectedGear(NEUTRAL);
    //setOpState(ENABLE);
    CK_milli = millis();
    setAlive();

    tickHandler.attach(this, CFG_TICK_INTERVAL_MOTOR_CONTROLLER_CK);
}

/*
 Finally, the firmware actually processes some of the status messages from the DmocMotorController
 However, currently the alive and checksum bytes aren't checked for validity.
 To be valid a frame has to have a different alive value than the last value we saw
 and also the checksum must match the one we calculate. Right now we'll just assume
 everything has gone according to plan.
 */
void CKMotorController::handleCanFrame(const CAN_message_t &frame) {
    int RotorTemp, invTemp, StatorTemp;
    int temp;
    setAlive(); //if a frame got to here then it passed the filter and must have been from the CK controller

    //Logger::debug("CKInverter CAN received: %X  %X  %X  %X  %X  %X  %X  %X  %X", frame->id,frame->data.bytes[0] ,frame->data.bytes[1],frame->data.bytes[2],frame->data.bytes[3],frame->data.bytes[4],frame->data.bytes[5],frame->data.bytes[6],frame->data.bytes[7]);

    switch (frame.id) {
    case 0x410: //Debugging output 1
        break;
    case 0x411: //debugging 2
        break;

    case 0x412: //debugging 3
        break;
    }
}

void CKMotorController::handleTick() {

    MotorController::handleTick(); //kick the ball up to papa

    checkAlive(1000);

    if (isOperational)
    {
        {
            //Logger::debug("EnableIn=%i and ReverseIn = %i" ,getEnableIn(),getReverseIn());
            //if(getEnableIn()<0) setOpState(ENABLE); //If we HAVE an enableinput 0-3, we'll let that handle opstate. Otherwise set it to ENABLE
            //if(getReverseIn()<0) setSelectedGear(DRIVE); //If we HAVE a reverse input, we'll let that determine forward/reverse.  Otherwise set it to DRIVE
        }
    }
    else {
        setSelectedGear(NEUTRAL); //We will stay in NEUTRAL until we get at least 40 frames ahead indicating continous communications.
    }

    sendPowerCmd();
}

//Commanded RPM plus state of key and gear selector
void CKMotorController::sendPowerCmd() {
    CKMotorControllerConfiguration *config = (CKMotorControllerConfiguration *)getConfiguration();
    CAN_message_t output;
    OperationState newstate;
    Gears currentGear = getSelectedGear();
    output.len = 7;
    output.id = 0x232;
    output.flags.extended = 0; //standard frame

	aliveCounter++;
	
	//obviously just for debugging during development. Do not leave these next lines here for long!
	//operationState = MotorController::ENABLE;
	//selectedGear = MotorController::DRIVE;
    setPowerMode(MotorController::modeSpeed);
	actualState = MotorController::ENABLE;

	if (operationState == ENABLE && currentGear != NEUTRAL)
	{
		if (getPowerMode() == modeSpeed)
		{
			torqueRequested = 0;
			if (throttleRequested > 0)
			{
		        speedRequested = (( throttleRequested * config->speedMax) / 1000.0f);
			}
			else speedRequested = 0;
		}	
		else if (getPowerMode() == modeTorque)
		{
			speedRequested = 0;
			torqueRequested = throttleRequested * config->torqueMax / 100.0f; //Send in 0.1Nm increments
		}
	}
	else
	{
		speedRequested = 0;
		torqueRequested = 0;
	}

	output.buf[0] = (speedRequested & 0x00FF);
    output.buf[1] = (speedRequested >> 8) & 0xFF;
    int16_t torq = torqueRequested * 10;
	output.buf[2] = (torq & 0x00FF);
	output.buf[3] = (torq >> 8) & 0xFF;    
   
    if (actualState == ENABLE) {
		output.buf[4] = 1;
		if (currentGear == MotorController::DRIVE) output.buf[4] += 2;
		if (currentGear == MotorController::REVERSE) output.buf[4] += 4;
    }
    else { //force neutral gear until the system is enabled.
        output.buf[4] = 0;
    }

	output.buf[5] = aliveCounter;

    output.buf[6] = calcChecksum(output);
	
	Logger::debug("CKInverter Sent Frame: %X  %X  %X  %X  %X  %X  %X  %X  %X", output.id, output.buf[0] , output.buf[1], output.buf[2], output.buf[3], output.buf[4], output.buf[5], output.buf[6]);

    canHandlerIsolated.sendFrame(output);
}

//just a bog standard CRC8 calculation with custom generator byte. Good enough.
//The point isn't obfuscation - just to prove to the other end that we're not insane and sending junk
//Obfuscation doesn't work well in open source projects anyway. ;)
byte CKMotorController::calcChecksum(CAN_message_t& thisFrame)
{
    const uint8_t generator = 0xAD;
    uint8_t crc = 0;

    for (int byt = 0; byt < 6; byt++)
    {
	    crc ^= thisFrame.buf[byt];

	    for (int i = 0; i < 8; i++)
	    {
		    if ((crc & 0x80) != 0)
		    {
			    crc = (uint8_t)((crc << 1) ^ generator);
		    }
		    else
		    {
			    crc <<= 1;
		    }
	    }
    }

    return crc;
}
	    
void CKMotorController::setGear(Gears gear) {
    setSelectedGear(gear);
    //if the gear was just set to drive or reverse and the DMOC is not currently in enabled
    //op state then ask for it by name
    if (gear != NEUTRAL) {
        operationState = ENABLE;
    }
    //should it be set to standby when selecting neutral? I don't know. Doing that prevents regen
    //when in neutral and I don't think people will like that.
}

uint32_t CKMotorController::getTickInterval()
{
    return CFG_TICK_INTERVAL_MOTOR_CONTROLLER_CK;
}

void CKMotorController::loadConfiguration() {
    CKMotorControllerConfiguration *config = (CKMotorControllerConfiguration *)getConfiguration();

    if (!config) {
        config = new CKMotorControllerConfiguration();
        setConfiguration(config);
    }

    MotorController::loadConfiguration(); // call parent
}

void CKMotorController::saveConfiguration() {
    MotorController::saveConfiguration();
}

DMAMEM CKMotorController ckMotorC;