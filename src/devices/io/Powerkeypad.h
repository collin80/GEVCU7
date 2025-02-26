/*
 * Powerkeypad.h - extended I/O module that interfaces with a powerkey pro keypad. Supports buttons, latching, and the LED lights
 *
Copyright (c) 2015 Collin Kidder, Michael Neuweiler, Charles Galpin

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

#pragma once
#include "../Device.h"
#include "../DeviceTypes.h"
#include "CANIODevice.h"

#define POWERKEYPRO 0x700

namespace LED
{
	enum LEDTYPE
	{
		OFF = 0,
		RED = 1,
		RED_BLINK = 2,
		RED_ALT_BLINK = 3,
		GREEN = 4,
		GREEN_BLINK = 5,
		GREEN_ALT_BLINK = 6,
		AMBER = 7,
		AMBER_BLINK = 8,
		AMBER_ALT_BLINK = 9,
		RED_GREEN_FLASH	= 10,
		AMBER_RED_FLASH = 12,
		GREEN_AMBER_FLASH = 14
	};
}

class PowerKPCANIODeviceConfiguration: public CanIODeviceConfiguration
{
public:
};

class PowerkeyPad : public CANIODevice
{
public:
	PowerkeyPad(void);

	bool getDigitalOutput(int which);
	int16_t getAnalogOutput(int which);
	void setDigitalOutput(int which, bool hi);	
	void setAnalogOutput(int which, int value);
	bool getDigitalInput(int which);
	int16_t getAnalogInput(int which);
	void setLEDState(int which, LED::LEDTYPE state);
	void sendLEDBatch();
	LED::LEDTYPE getLEDState(int which);
	void setLatchingMode(int which, LatchModes::LATCHMODE mode);
	void unlockLatch(int which);
	void sendAutoStart();

    void loadConfiguration();
    void saveConfiguration();
	void setup();

	void handleCanFrame(const CAN_message_t &frame);
	void handlePDOFrame(CAN_message_t &frame);
	void handleSDORequest(SDO_FRAME &frame);
	void handleSDOResponse(SDO_FRAME &frame);

    void handleMessage(uint32_t, void*);

private:
	int deviceID;
	bool buttonState[12]; //The reported state of each button (might be a lie for some modes)
	bool actualState[12]; //the current actual state of each button
	bool toggleState[12]; //used by any inputs set to LatchModes::TOGGLING
	LED::LEDTYPE LEDState[12]; //LED state for all 12 keys
	LatchModes::LATCHMODE latchState[12];
};