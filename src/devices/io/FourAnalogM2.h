/*
 * FourAnalogM2.h - Implements an interface to an M.2 expansion card available for GEVCU7. Implements 4 analog outputs 0-5v
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
#include "ExtIODevice.h"

#define FOURANALOGM2 0x710

class FourAnaM2DeviceConfiguration: public ExtIODeviceConfiguration
{
public:
};

class FourAnalogM2 : public ExtIODevice
{
public:
	FourAnalogM2(void);

	int16_t getAnalogOutput(int which);
	void setAnalogOutput(int which, int value);

    void loadConfiguration();
    void saveConfiguration();
	void setup();

    void handleMessage(uint32_t, void*);

private:
	int16_t values[4];
	FourAnaM2DeviceConfiguration *config;
};