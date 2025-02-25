/*
 * sys_io.cpp
 *
 * Handles the low level details of system I/O
 *
Copyright (c) 2013 Collin Kidder, Michael Neuweiler, Charles Galpin

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

#include "sys_io.h"
#include "devices/io/ExtIODevice.h"
#include "devices/misc/SystemDevice.h"
#include "i2c_driver_wire.h"
#include "DeviceManager.h"

#undef HID_ENABLED

SystemIO::SystemIO() : Device()
{
    commonName = "System IO";
    shortName = "IO";
    for (int i = 0; i < NUM_EXT_IO; i++)
    {
        extendedDigitalOut[i].device = NULL;
        extendedDigitalIn[i].device = NULL;
        extendedAnalogOut[i].device = NULL;
        extendedAnalogIn[i].device = NULL;
    }
    
    numDigIn = NUM_DIGITAL;
    numDigOut = NUM_OUTPUT;
    numAnaIn = NUM_ANALOG;
    numAnaOut = 0;
    pcaDigitalOutputCache = 0; //all outputs off by default
    adcMuxSelect = 0;
    ioStatusIdx = 0;

    for (int i = 0; i < NUM_OUTPUT; i++)
    {
        digPWMOutput[i].triggerPoint = 0;
        digPWMOutput[i].progress = 0;
        digPWMOutput[i].pwmActive = false;
        digPWMOutput[i].freqInterval = 0;
        digOutState[i] = 0;

    }
    
    for (int i = 0; i < NUM_DIGITAL; i++) digInState[i] = 0;
    for (int i = 0; i < NUM_ANALOG; i++) anaInState[i] = 0;
    
    adc = new ADC(); // adc object

    ranSetup = false;
}

void SystemIO::setup_ADC_params()
{
    for (int i = 0; i < 8; i++) 
        Logger::debug( "ADC:%d GAIN: %d Offset: %d", i, sysConfig->adcGain[i], sysConfig->adcOffset[i] );
}

FLASHMEM void SystemIO::setSystemType(SystemType systemType) {
    if (systemType >= GEVCU7A && systemType <= GEVCU7C)
    {
        sysConfig->systemType = systemType;
        Device *sysDev;
        sysDev = deviceManager.getDeviceByID(SYSTEM);
        sysDev->saveConfiguration();
    }
}

SystemType SystemIO::getSystemType() {
    return sysType;
}

void SystemIO::earlyInit()
{
    if (!prefsHandler) prefsHandler = new PrefHandler(SYSIO);
}

DeviceId SystemIO::getId() {
    return (SYSIO);
}

DeviceType SystemIO::getType()
{
    return DEVICE_IO;
}

FLASHMEM void SystemIO::setup() {
    if (ranSetup) return;
    analogReadRes(12);
    tickHandler.detach(this);

    setup_ADC_params(); //brings up adc calibration params. Optional if you want to tune them

    pinMode(9, INPUT); //these 4 are digital inputs not
    pinMode(40, INPUT); //connected to PCA chip. They're direct
    pinMode(41, INPUT); //basically the last 4 digital inputs
    pinMode(42, INPUT);

    pinMode(3, OUTPUT); //PWM0 = ADC Select A
    digitalWrite(3, LOW); 
    if (sysConfig->systemType != GEVCU7B)
    {
        pinMode(2, OUTPUT); //PWM1 = ADC Select B
        digitalWrite(2, LOW); //both off by default to select mux 0
    }
    else
    {
        Logger::debug("GEVCU7B detected. Using work around analog IO");
        pinMode(2, INPUT); //input won't mess up the CAN line this got crossed with
        pinMode(6, OUTPUT); //ESP32 boot pin - kludge to allow prototype to work still
        digitalWrite(6, LOW); //both off by default to select mux 0
    }

    initDigitalMultiplexor(); //set I/O direction for all pins, polarity, etc.

    tickHandler.attach(this, 1000); //interval is set in microseconds. We want 1ms timer
    lastMicros = micros();

    pinMode(A0, INPUT);
    pinMode(A1, INPUT);

    //not using traditional Arduino analog routines. These special routines give better performance
    adc->adc0->setAveraging(4);                                             // set number of averages
    adc->adc0->setResolution(12);                                           // set bits of resolution
    adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED );       // change the conversion speed
    adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED );           // change the sampling speed
    ////// ADC1 /////
    adc->adc1->setAveraging(4);                                             // set number of averages
    adc->adc1->setResolution(12);                                           // set bits of resolution
    adc->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED);       // change the conversion speed
    adc->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED );           // change the sampling speed

    setupStatusEntries();

    ranSetup = true;
    //the need for ranSetup is based upon a kludge. This is set up as a device now but it gets initialized
    //early so setup is called manually. But, then the device manager tries again later on thinking it hasn't
    //been done. The system device has this same problem. It should be fixed properly at some point.
}


/* The status system acts as a central storehouse of all status related data in the whole system. It is
   very likely someone might want to know the current status of all the I/O so we add entries for it all here.
*/
FLASHMEM void SystemIO::setupStatusEntries()
{
    char buff[30];
    int i;

    StatusEntry stat;

    for (i = 0; i < NUM_DIGITAL; i++)
    {
        sprintf(buff, "SYS_DIGIN%i", i);
      //        name       var           type                  prevVal  obj
        stat = {buff, &digInState[i], CFG_ENTRY_VAR_TYPE::BYTE, 0, this};
        deviceManager.addStatusEntry(stat);
    }

    for (i = 0; i < NUM_OUTPUT; i++)
    {
        sprintf(buff, "SYS_DIGOUT%i", i);
      //        name       var           type                  prevVal  obj
        stat = {buff, &digOutState[i], CFG_ENTRY_VAR_TYPE::BYTE, 0, this};
        deviceManager.addStatusEntry(stat);
    }

    for (i = 0; i < NUM_ANALOG; i++)
    {
        sprintf(buff, "SYS_ANAIN%i", i);
      //        name       var           type                  prevVal  obj
        stat = {buff, &anaInState[i], CFG_ENTRY_VAR_TYPE::INT16, 0, this};
        deviceManager.addStatusEntry(stat);
    }
}

void SystemIO::installExtendedIO(ExtIODevice *device)
{
    int counter;
    
    Logger::avalanche("Before adding extended IO counts are DI:%i DO:%i AI:%i AO:%i", numDigIn, numDigOut, numAnaIn, numAnaOut);
    Logger::avalanche("Num Analog Inputs: %i", device->getAnalogInputCount());
    Logger::avalanche("Num Analog Outputs: %i", device->getAnalogOutputCount());
    Logger::avalanche("Num Digital Inputs: %i", device->getDigitalInputCount());
    Logger::avalanche("Num Digital Outputs: %i", device->getDigitalOutputCount());
   
    if (device->getAnalogInputCount() > 0)
    {
        Logger::avalanche("This device has analog inputs.");
        for (counter = 0; counter < NUM_EXT_IO; counter++)
        {
            if (extendedAnalogIn[counter].device == NULL)
            {
                for (int i = 0; i < device->getAnalogInputCount(); i++)
                {
                    if ((counter + i) == NUM_EXT_IO) break;
                    extendedAnalogIn[counter + i].device = device;
                    extendedAnalogIn[counter + i].localOffset = i;
                }
                break;
            }
        }
    }
    
    if (device->getAnalogOutputCount() > 0)
    {
        Logger::avalanche("This device has analog outputs.");
        for (counter = 0; counter < NUM_EXT_IO; counter++)
        {
            if (extendedAnalogOut[counter].device == NULL)
            {
                for (int i = 0; i < device->getAnalogOutputCount(); i++)
                {
                    if ((counter + i) == NUM_EXT_IO) break;
                    extendedAnalogOut[counter + i].device = device;
                    extendedAnalogOut[counter + i].localOffset = i;
                }
                break;
            }
        }
    }

    if (device->getDigitalOutputCount() > 0)
    {
        Logger::avalanche("This device has digital outputs.");
        for (counter = 0; counter < NUM_EXT_IO; counter++)
        {
            if (extendedDigitalOut[counter].device == NULL)
            {
                for (int i = 0; i < device->getDigitalOutputCount(); i++)
                {
                    if ((counter + i) == NUM_EXT_IO) break;
                    extendedDigitalOut[counter + i].device = device;
                    extendedDigitalOut[counter + i].localOffset = i;
                }
                break;
            }
        }
    }

    if (device->getDigitalInputCount() > 0)
    {
        Logger::avalanche("This device has digital inputs.");
        for (counter = 0; counter < NUM_EXT_IO; counter++)
        {
            if (extendedDigitalIn[counter].device == NULL)
            {
                for (int i = 0; i < device->getDigitalInputCount(); i++)
                {
                    if ((counter + i) == NUM_EXT_IO) break;
                    extendedDigitalIn[counter + i].device = device;
                    extendedDigitalIn[counter + i].localOffset = i;
                }
                break;
            }
        }
    }
    
    int numDI = NUM_DIGITAL; 
    for (int i = 0; i < NUM_EXT_IO; i++)
    {
        if (extendedDigitalIn[i].device != NULL) numDI++;
        else break;
    }
    numDigIn = numDI;
    
    int numDO = NUM_OUTPUT;
    for (int i = 0; i < NUM_EXT_IO; i++)
    {
        if (extendedDigitalOut[i].device != NULL) numDO++;
        else break;
    }
    numDigOut = numDO;

    int numAI = NUM_ANALOG; 
    for (int i = 0; i < NUM_EXT_IO; i++)
    {
        if (extendedAnalogIn[i].device != NULL) numAI++;
        else break;
    }
    numAnaIn = numAI;

    int numAO = 0; //GEVCU has no real analog outputs - there are PWM but they're on the digital outputs
    for (int i = 0; i < NUM_EXT_IO; i++)
    {
        if (extendedAnalogOut[i].device != NULL) numAO++;
        else break;
    }
    numAnaOut = numAO;
    Logger::debug("After added extended IO the counts are DI:%i DO:%i AI:%i AO:%i", numDigIn, numDigOut, numAnaIn, numAnaOut);
}

int SystemIO::numDigitalInputs()
{
    return numDigIn;
}

int SystemIO::numDigitalOutputs()
{
    return numDigOut;
}

int SystemIO::numAnalogInputs()
{
    return numAnaIn;
}

int SystemIO::numAnalogOutputs()
{
    return numAnaOut;
}

int16_t SystemIO::_pGetAnalogRaw(uint8_t which)
{
    int32_t valu;

    if (which >= NUM_ANALOG)
    {
        return 0;
    }
        
    int neededMux = which % 4;
    //we don't change the analog mux unless absolutely necessary
    if (neededMux != adcMuxSelect) //must change mux to read this
    {
        if (sysConfig->systemType != GEVCU7B)
        {
            digitalWrite(2, (neededMux & 2) ? HIGH : LOW);
        }
        else 
        {
            digitalWrite(6, (neededMux & 2) ? HIGH : LOW);
        }

        digitalWrite(3, (neededMux & 1) ? HIGH : LOW);
        //Logger::debug("ADC for %u mux1 %u mux2 %u", which, (neededMux & 1), (neededMux & 2));
        adcMuxSelect = neededMux;
        //the analog multiplexor input switch pins are on direct outputs from the teensy
        //and so will change very rapidly. The multiplexor also switches inputs in less than
        //1 microsecond. The inputs are all buffered with 1uF caps and so perhaps the slowest
        //part of switching is allowing the analog input pin on the teensy to settle
        //to the new value of the analog input. Even so, this should happen very rapidly.
        //Perhaps only 1us goes by between switching the mux and the new value appearing
        //but give it at least a few microseconds for everything to switch and the new value to
        //settle at the Teensy ADC pin.
        delayMicroseconds(5);
        //delay(6); //really long delay!

    }
    //Analog inputs 0-3 are always on ADC0, 4-7 are on ADC1
    if (which < 4) 
    {
        //valu = analogRead(0);
        valu = adc->adc0->analogRead(0);
        //Logger::debug("AREAD0: %u", valu);
    }
    else 
    {
        //valu = analogRead(1);
        valu = adc->adc1->analogRead(1);
        //Logger::debug("AREAD1: %u", valu);
    }
    return valu;
}


/*
get value of one of the analog inputs
*/
int16_t SystemIO::getAnalogIn(uint8_t which) {
    int valu;

    if (which > numAnaIn)
    {
        return 0;
    }
        
    if (which < NUM_ANALOG)
    {
        valu = _pGetAnalogRaw(which);
        valu -= sysConfig->adcOffset[which];
        valu = (valu * sysConfig->adcGain[which]) / 1024;
        return valu;
    }
    else //the return makes this superfluous...
    {        
        //handle an extended I/O call
        ExtIODevice *dev = extendedAnalogIn[which - NUM_ANALOG].device;
        if (dev) return dev->getAnalogInput(extendedAnalogIn[which - NUM_ANALOG].localOffset);
        return 0;
    }
    return 0; //if it falls through and nothing could provide the answer then return 0
}

//there really are no directly connected analog outputs but extended I/O devices might implement some
boolean SystemIO::setAnalogOut(uint8_t which, int32_t level)
{
    if (which >= numAnaOut) return false;
    ExtIODevice *dev;
    dev = extendedAnalogOut[which].device;
    if (dev) dev->setAnalogOutput(extendedAnalogOut[which].localOffset, level);
    return true;   
}

int32_t SystemIO::getAnalogOut(uint8_t which)
{
    if (which >= numAnaOut) return 0;
    ExtIODevice *dev;
    dev = extendedAnalogOut[which].device;
    if (dev) return dev->getAnalogOutput(extendedAnalogOut[which].localOffset);
    return 0;    
}

//get value of one of the 12 digital inputs (or more if extended I/O added more)
boolean SystemIO::getDigitalIn(uint8_t which) {
    if (which >= numDigIn) return false;
    
    if (which < NUM_DIGITAL) 
    {
        if (which < 8) return _pGetDigitalInput(which);
        else 
        {
            switch (which)
            {
            case 8:
                return !(digitalRead(40));
                break;
            case 9:
                return !(digitalRead(41));
                break;
            case 10:
                return !(digitalRead(42));
                break;
            case 11:
                return !(digitalRead(9));
                break;

            }
        }
    }
    else
    {
        ExtIODevice *dev;
        dev = extendedDigitalIn[which - NUM_DIGITAL].device;
        if (dev) return dev->getDigitalInput(extendedDigitalIn[which - NUM_DIGITAL].localOffset);
    }
    return false;
}

//set output high or not
void SystemIO::setDigitalOutput(uint8_t which, boolean active) {
    if (which >= numDigOut) return;
    
    if (which < NUM_OUTPUT)
    {
        _pSetDigitalOutput(which, active);
        digPWMOutput[which].pwmActive = false;
        digOutState[which] = active;
    }
    else
    {
        ExtIODevice *dev;
        dev = extendedDigitalOut[which - NUM_OUTPUT].device;
        if (dev) return dev->setDigitalOutput(extendedDigitalOut[which - NUM_OUTPUT].localOffset, active);
    }
}

//get current value of output state (high?)
boolean SystemIO::getDigitalOutput(uint8_t which) {
    if (which >= numDigOut) return false;
    
    if (which < NUM_OUTPUT)
    {
        return _pGetDigitalOutput(which);
    }
    else
    {
        ExtIODevice *dev;
        dev = extendedDigitalOut[which - NUM_OUTPUT].device;
        if (dev) return dev->getDigitalOutput(extendedDigitalOut[which - NUM_OUTPUT].localOffset);
    }
    return false;
}

//Which is 0-7 to specify which output to turn into PWM
//Freq is 1-255 to set frequency in hertz. Really only works well up to maybe 60Hz
//Duty is in tenths of a percent 0-1000
//Calling this function causes the output to become PWM. Calling setDigitalOutput
//turns off PWM on the given output
void SystemIO::setDigitalOutputPWM(uint8_t which, uint8_t freq, uint16_t duty)
{
    if (which >= NUM_OUTPUT) return;
    if (duty > 1000) return;
    if (freq == 0) return;
    digPWMOutput[which].progress = 0;
    digPWMOutput[which].pwmActive = true;
    digPWMOutput[which].freqInterval = 1000000ul / freq; //# of uS in each full cycle
    //now, how far into the progress do we need to get before turning on the output?
    double prog = duty / 1000.0;
    digPWMOutput[which].triggerPoint = (uint32_t)(digPWMOutput[which].freqInterval * prog);
    _pSetDigitalOutput(which, false);//set it low to start
    digOutState[which] = 0;
}

void SystemIO::updateDigitalPWMDuty(uint8_t which, uint16_t duty)
{
    if (which >= NUM_OUTPUT) return;
    if (duty > 1000) return;
    double prog = duty / 1000.0;
    digPWMOutput[which].triggerPoint = (uint32_t)(digPWMOutput[which].freqInterval * prog);
}

void SystemIO::updateDigitalPWMFreq(uint8_t which, uint8_t freq)
{
    if (which >= NUM_OUTPUT) return;
    if (freq == 0) return;
    uint32_t newval = 1000000ul / freq;
    double ratio = (double)newval / (double)digPWMOutput[which].freqInterval;
    digPWMOutput[which].freqInterval = newval; //# of uS in each full cycle
    digPWMOutput[which].triggerPoint *= ratio;
}

/*
A lot was precalculated to save time here. Just figure out how much time has passed since the last call
then add that to the progress value of each enabled PWM output. If the result goes over the trigger threshold
then set the output high, otherwise set it low. This should be pretty stable for any PWM not right near the two
ends of the spectrum. But, given our crappy resolution, none of this will work great if the PWM frequency is too
high or the duty is at either end. Frequencies under 40Hz should be OK and duty cycles between 10 and 90 percent
are probably fine. This is sufficient to drive the PWM of water pumps or the Tesla water heater, probably OK
for gauges too.
*/
void SystemIO::handleTick()
{
    uint32_t now = micros();
    uint32_t interval = now - lastMicros;
    lastMicros = now;
    uint8_t outputMask;
    uint8_t tempCache = pcaDigitalOutputCache;

    //each tick increment status index and maybe update one of the digital or analog inputs
    if (ioStatusIdx < NUM_DIGITAL)
    {
        digInState[ioStatusIdx] = getDigitalIn(ioStatusIdx);
    }
    else if ((ioStatusIdx - NUM_DIGITAL) < NUM_ANALOG)
    {
        int idx = ioStatusIdx - NUM_DIGITAL;
        anaInState[idx] = getAnalogIn(idx);
    }
    else ioStatusIdx = -1;
    ioStatusIdx++;

    for (int i = 0; i < NUM_OUTPUT; i++)
    {
        if (!digPWMOutput[i].pwmActive) continue;
        digPWMOutput[i].progress += interval;
        //Logger::debug("%i: %u %u %u", i, digPWMOutput[i].progress, digPWMOutput[i].freqInterval, digPWMOutput[i].triggerPoint);        
        if (digPWMOutput[i].progress >= digPWMOutput[i].triggerPoint)
        {
            Logger::debug("%i on!", i);
            pcaDigitalOutputCache |= (1 << i);
            digOutState[i] = 1;
        }
        else
        {
            Logger::debug("%i OFF!", i);
            outputMask = ~(1 << i);
            pcaDigitalOutputCache &= outputMask;
            digOutState[i] = 0;
        }
        //we have to constrain the progress variable to be within the freqInterval value but do so here
        //after we've already done our output calc because this should yield the closest match to our
        //desired pulse width. But, still the pulse width is likely to jitter by +/- 1ms
        if (digPWMOutput[i].progress > digPWMOutput[i].freqInterval) digPWMOutput[i].progress -= digPWMOutput[i].freqInterval;
    }

    //only update the chip if we actually changed anything
    if (pcaDigitalOutputCache != tempCache)
    {
        Wire.beginTransmission(PCA_ADDR);
        Wire.write(PCA_WRITE_OUT0);
        Wire.write(pcaDigitalOutputCache);
        Wire.endTransmission();
    }
}

/*
 * adc is the adc port to calibrate, update if true will write the new value to EEPROM automatically
 */
bool SystemIO::calibrateADCOffset(int adc, bool update)
{
    int32_t accum = 0;
    
    if (adc < 0 || adc > 7) return false;
    
    for (int j = 0; j < 500; j++)
    {
        accum += _pGetAnalogRaw(adc);
        //normally one shouldn't call watchdog reset in multiple
        //places but this is a special case.
        //watchdogReset();
        delay(2);
    }
    accum /= 500;
    sysConfig->adcOffset[adc] = accum;
    Logger::console("ADC %i offset is now %i", adc, accum);
    return true;
}


//much like the above function but now we use the calculated offset and take readings, average them
//and figure out how to set the gain such that the average reading turns up to be the target value
bool SystemIO::calibrateADCGain(int adc, int32_t target, bool update)
{
    int32_t accum = 0;
    
    if (adc < 0 || adc > 7) return false;
    
    for (int j = 0; j < 500; j++)
    {
        accum += _pGetAnalogRaw(adc);

        //normally one shouldn't call watchdog reset in multiple
        //places but this is a special case.
        //watchdogReset();
        delay(2);
    }
    accum /= 500;
    Logger::console("Unprocessed accum: %i", accum);
    
    //now apply the proper offset we've got set.
    if (adc < 8) 
    {
        accum /= 2048;
        accum -= sysConfig->adcOffset[adc];
    }

    if ((target / accum) > 20) {
        Logger::console("Calibration not possible. Check your target value.");
        return false;
    }
    
    if (accum < 1000 && accum > -1000) {
        Logger::console("Readings are too low. Try applying more voltage/current");
        return false;
    }
    
    //1024 is one to one so all gains are multiplied by that much to bring them into fixed point math.
    //we've got a reading accum and a target. The rational gain is target/accum
    sysConfig->adcGain[adc] = (int16_t)((16384ull * target) / accum);    
    Logger::console("Accum: %i    Target: %i", accum, target);
    Logger::console("ADC %i gain is now %i", adc, sysConfig->adcGain[adc]);
    return true;
}

FLASHMEM void SystemIO::initDigitalMultiplexor()
{
    //all of port 0 are outputs, all of port 1 are inputs
    //1 in a config bit means input, 0 = output
    Wire.begin();

    Wire.beginTransmission(PCA_ADDR);  // setup to write to PCA chip
    Wire.write(PCA_WRITE_OUT0);
    Wire.write(0); //all outputs should start out OFF!
    Wire.endTransmission();

    Wire.beginTransmission(PCA_ADDR);  // setup to write to PCA chip
    Wire.write(PCA_CFG_0);
    Wire.write(0); //all zeros means all outputs
    Wire.endTransmission();

    Wire.beginTransmission(PCA_ADDR);  // setup to write to PCA chip
    Wire.write(PCA_CFG_1);
    Wire.write(0xFF); //all 1's means all inputs
    Wire.endTransmission();

    Wire.beginTransmission(PCA_ADDR);  // setup to write to PCA chip
    Wire.write(PCA_POLARITY_1);
    Wire.write(0xFF); //all inputs are active low so invert all those
    Wire.endTransmission();
    
}

int SystemIO::_pGetDigitalInput(int pin) //all inputs are on port 1
{
    if ( (pin < 0) || (pin > 7) ) return 0;
    Wire.beginTransmission(PCA_ADDR);
    Wire.write(PCA_READ_IN1);
    Wire.endTransmission();

    Wire.requestFrom(PCA_ADDR, 1); //get one byte
    if (Wire.available())
    {
        uint8_t c = Wire.read();
        c = (c >> pin) & 1;
        return c;
    }
    return 0; //fallback in case something messes up
}

void SystemIO::_pSetDigitalOutput(int pin, int state)
{
    if ( (pin < 0) || (pin > 7) ) return;

    uint8_t outputMask = ~(1<<pin);
    pcaDigitalOutputCache &= outputMask;
    if (state != 0) pcaDigitalOutputCache |= (1<<pin);

    Wire.beginTransmission(PCA_ADDR);
    Wire.write(PCA_WRITE_OUT0);
    Wire.write(pcaDigitalOutputCache);
    Wire.endTransmission();
}

int SystemIO::_pGetDigitalOutput(int pin)
{
    if ( (pin < 0) || (pin > 7) ) return 0;
    Wire.beginTransmission(PCA_ADDR);
    Wire.write(PCA_READ_IN0);
    Wire.endTransmission();

    Wire.requestFrom(PCA_ADDR, 1); //get one byte
    if (Wire.available())
    {
        uint8_t c = Wire.read();
        c = (c >> pin) & 1;
        return c;
    }
    return 0; //fallback in case something messes up
}

DMAMEM SystemIO systemIO;


