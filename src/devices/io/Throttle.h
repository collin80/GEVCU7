/*
 * Throttle.h
 *
 * Parent class for all throttle controllers, be they canbus or pot or hall effect, etc
 * This class is virtually totally virtual. The derived classes redefine most everything
 * about this class. It might even be a good idea to make the class totally abstract.


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

#ifndef THROTTLE_H_
#define THROTTLE_H_

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "../Device.h"

#define THROTTLE 0x1030

enum THROTTLE_FAULTS
{
    THROTTLE_FAULT_IN1_TOOHIGH = 1000,
    THROTTLE_FAULT_IN1_TOOLOW,
    THROTTLE_FAULT_IN2_TOOHIGH,
    THROTTLE_FAULT_IN2_TOOLOW,
    THROTTLE_FAULT_IN3_TOOHIGH,
    THROTTLE_FAULT_IN3_TOOLOW,
    THROTTLE_FAULT_MISMATCH,   
    THROTTLE_LAST_FAULT
};

static const char* THROTTLE_FAULT_DESCS[] =
{
    "Throttle input 1 is too high",
    "Throttle input 1 is too low",
    "Throttle input 2 is too high",
    "Throttle input 2 is too low",
    "Throttle input 3 is too high",
    "Throttle input 3 is too low",
    "Throttle inputs do not agree on position",
};

//These should be able to be removed.
/*
#define ThrottleRegenMinValue	270		//where does Regen stop (1/10 of percent)
#define ThrottleRegenMaxValue	0		//where Regen is at maximum (1/10 of percent)
#define ThrottleFwdValue		280		//where does forward motion start
#define ThrottleMapValue		750		//Where is the 1/2 way point for throttle
#define ThrottleMinRegenValue	0		//how many percent of full power to use at minimal regen
#define ThrottleMaxRegenValue	70		//how many percent of full power to use at maximum regen
#define ThrottleCreepValue		0		//how many percent of full power to use at creep
#define BrakeMinValue			100		//Value ADC reads when brake is not pressed
#define BrakeMaxValue			3200		//Value ADC reads when brake is pushed all of the way down
#define BrakeMinRegenValue		0		//percent of full power to use for brake regen (min)
#define BrakeMaxRegenValue		50		//percent of full power to use for brake regen (max)
#define BrakeADC				0       //which ADC pin to use
*/
//these two should be configuration options instead.
#define CFG_CANTHROTTLE_MAX_NUM_LOST_MSG            3 // maximum number of lost messages allowed
#define CFG_THROTTLE_TOLERANCE  150 //the max that things can go over or under the min/max without fault - 1/10% each #
#define ThrottleMaxErrValue		150		//tenths of percentage allowable deviation between pedals

/*
 * Data structure to hold raw signal(s) of the throttle.
 * E.g. for a three pot pedal, all signals could be used.
 */
struct RawSignalData {
    int32_t input1; // e.g. pot #1 or the signal from a can bus throttle
    int32_t input2; // e.g. pot #2 (optional)
    int32_t input3; // e.g. pot #3 (optional)
};

struct ThrottleMapPoint {
    uint16_t inputPosition;
    uint16_t outputPosition;
};

/*
 * A abstract class to hold throttle configuration parameters.
 * Can be extended by the subclass.
 */
class ThrottleConfiguration: public DeviceConfiguration {
public:
    uint16_t positionRegenMaximum, positionRegenMinimum; // throttle position where regen is highest and lowest
    uint16_t positionForwardMotionStart; // throttle position where forward motion starts and the mid point of throttle
    ThrottleMapPoint mapPoints[3];
    uint8_t maximumRegen; // percentage of max torque allowable for regen at maximum level
    uint8_t minimumRegen; // percentage of max torque allowable for regen at minimum level
    uint8_t creep; // percentage of torque used for creep function (imitate creep of automatic transmission, set 0 to disable)
    float smoothingVal;
    uint16_t slewRate;
    uint16_t slewDecel;
    uint16_t slewCutoff;
};

/*
 * Abstract class for all throttle implementations.
 */
class Throttle: public Device {
public:
    enum ThrottleStatus {
        OK,
        ERR_LOW_T1,
        ERR_LOW_T2,
        ERR_HIGH_T1,
        ERR_HIGH_T2,
        ERR_MISMATCH,
        ERR_MISC
    };

    Throttle();
    virtual int16_t getLevel();
    void handleTick();
    virtual ThrottleStatus getStatus();
    virtual bool isFaulted();
    virtual void setup();
    virtual const char* getFaultDescription(uint16_t faultcode);
    
    virtual RawSignalData *acquireRawSignal();
    void loadConfiguration();
    void saveConfiguration();

protected:
    ThrottleStatus status;
    virtual bool validateSignal(RawSignalData *);
    virtual int16_t calculatePedalPosition(RawSignalData *);
    virtual int16_t mapPedalPosition(int16_t);
    int16_t normalizeAndConstrainInput(int32_t, int32_t, int32_t);
    int32_t normalizeInput(int32_t, int32_t, int32_t);    

private:
    int16_t level; // the final signed throttle level. [-1000, 1000] in permille of maximum
    int16_t pedalPosition;
    int16_t rawThrottle;
    RawSignalData lastVal;
};

#endif


