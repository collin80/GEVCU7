/*
 * constants.h
 *
 * Defines the global / application wide constants
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
 *      Author: Michael Neuweiler
 */

#ifndef CONSTANTS_H_
#define CONSTANTS_H_

namespace Constants {
// misc
extern const char* trueStr;
extern const char* falseStr;
extern const char* notAvailable;

// configuration

extern const char* numThrottlePots;
extern const char* throttleSubType;
extern const char* throttleMin1;
extern const char* throttleMin2;
extern const char* throttleMax1;
extern const char* throttleMax2;
extern const char* throttleRegenMax;
extern const char* throttleRegenMin;
extern const char* throttleFwd;
extern const char* throttleMap;
extern const char* throttleMinRegen;
extern const char* throttleMaxRegen;
extern const char* throttleCreep;
extern const char* brakeMin;
extern const char* brakeMax;
extern const char* brakeMinRegen;
extern const char* brakeMaxRegen;
extern const char* brakeLight;
extern const char* revLight;
extern const char* enableIn;
extern const char* reverseIn;

extern const char* speedMax;
extern const char* torqueMax;
extern const char* logLevel;

// status
extern const char* timeRunning;
extern const char* torqueRequested;
extern const char* torqueActual;
extern const char* throttle;
extern const char* brake;
extern const char* motorMode;
extern const char* speedRequested;
extern const char* speedActual;
extern const char* dcVoltage;
extern const char* nominalVolt;
extern const char* dcCurrent;
extern const char* acCurrent;
extern const char* kiloWattHours;
extern const char* bitfield1;
extern const char* bitfield2;
extern const char* bitfield3;
extern const char* bitfield4;
extern const char* running;
extern const char* faulted;
extern const char* warning;
extern const char* gear;
extern const char* tempMotor;
extern const char* tempInverter;
extern const char* tempSystem;
extern const char* mechPower;
extern const char* prechargeR;
extern const char* prechargeRelay;
extern const char* mainContactorRelay;
extern const char* coolFan;
extern const char* coolOn;
extern const char* coolOff;
extern const char* validChecksum;
extern const char* invalidChecksum;
extern const char* valueOutOfRange;
extern const char* normalOperation;
}
#endif /* CONSTANTS_H_ */


