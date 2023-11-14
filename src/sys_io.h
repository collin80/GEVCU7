/*
 * sys_io.h
 *
 * Handles raw interaction with system I/O
 *
Copyright (c) 2021 Collin Kidder, Michael Neuweiler, Charles Galpin

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


#ifndef SYS_IO_H_
#define SYS_IO_H_

#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include "eeprom_layout.h"
#include "PrefHandler.h"
#include "Logger.h"
#include <ADC.h> //better ADC library compared to the built-in ADC functions
#include "TickHandler.h"
#include "devices/Device.h"

class ExtIODevice;

enum SystemType 
{
    GEVCU7A = 0,
    GEVCU7B = 1,
    GEVCU7C = 2
};

class ExtendedIODev
{
public:
    ExtIODevice *device;
    uint8_t localOffset;
};

//Base address 0x20. Then A0-A2 set lower 3 bits so 0x20-0x27 but only A0 is high on GEVCU7
#define PCA_ADDR    0x21

#define PCA_READ_IN0    0
#define PCA_READ_IN1    1
#define PCA_WRITE_OUT0  2
#define PCA_WRITE_OUT1  3
#define PCA_POLARITY_0  4
#define PCA_POLARITY_1  5
#define PCA_CFG_0       6
#define PCA_CFG_1       7
#define PCA_WRITE       0
#define PCA_READ        1

struct PWM_SPECS
{
    uint32_t freqInterval;
    uint32_t triggerPoint;
    bool pwmActive;
    uint32_t progress;
};

class SystemIO: public Device
{
public:
    SystemIO();
    
    void setup();
    void earlyInit();
    void setup_ADC_params();

    DeviceId getId();
    DeviceType getType();

    int16_t getAnalogIn(uint8_t which); //get value of one of the 4 analog inputs
    boolean setAnalogOut(uint8_t which, int32_t level);
    int32_t getAnalogOut(uint8_t which);
    boolean getDigitalIn(uint8_t which); //get value of one of the 4 digital inputs
    void setDigitalOutput(uint8_t which, boolean active); //set output high or not
    boolean getDigitalOutput(uint8_t which); //get current value of output state (high?)

    void setDigitalOutputPWM(uint8_t which, uint8_t freq, uint16_t duty);
    void updateDigitalPWMDuty(uint8_t which, uint16_t duty);
    void updateDigitalPWMFreq(uint8_t which, uint8_t freq);
    void handleTick();

    void setDigitalInLatchMode(int which, LatchModes::LATCHMODE mode);
    void unlockDigitalInLatch(int which);

    void installExtendedIO(ExtIODevice *device);

    int numDigitalInputs();
    int numDigitalOutputs();
    int numAnalogInputs();
    int numAnalogOutputs();

    void setSystemType(SystemType);
    SystemType getSystemType();
    bool calibrateADCOffset(int, bool);
    bool calibrateADCGain(int, int32_t, bool);

private:
    void initDigitalMultiplexor();
    int _pGetDigitalInput(int pin);
    void _pSetDigitalOutput(int pin, int state);
    int _pGetDigitalOutput(int pin);
    int16_t _pGetAnalogRaw(uint8_t which);
    void setupStatusEntries();

    ADC *adc;

    SystemType sysType;

    bool ranSetup;

    int adcMuxSelect;

    int ioStatusIdx;

    uint8_t pcaDigitalOutputCache;
    
    int numDigIn;
    int numDigOut;
    int numAnaIn;
    int numAnaOut;

    uint8_t digOutState[NUM_OUTPUT];
    uint8_t digInState[NUM_DIGITAL];
    int16_t anaInState[NUM_ANALOG];

    PWM_SPECS digPWMOutput[NUM_OUTPUT];
    uint32_t lastMicros;
    
    ExtendedIODev extendedDigitalOut[NUM_EXT_IO];
    ExtendedIODev extendedDigitalIn[NUM_EXT_IO];
    ExtendedIODev extendedAnalogOut[NUM_EXT_IO];
    ExtendedIODev extendedAnalogIn[NUM_EXT_IO];
};

extern SystemIO systemIO;

#endif
