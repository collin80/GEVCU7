/*
 * MotorController.h
  *
 * Parent class for all motor controllers.
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

#ifndef MOTORCTRL_H_
#define MOTORCTRL_H_

#include <Arduino.h>
#include "../../config.h"
#include "../Device.h"
#include "../io/Throttle.h"
#include "../../DeviceManager.h"
#include "../../sys_io.h"

#define MOTORCTL_INPUT_DRIVE_EN    3
#define MOTORCTL_INPUT_FORWARD     4
#define MOTORCTL_INPUT_REVERSE     5
#define MOTORCTL_INPUT_LIMP        6

#define CFG_TICK_INTERVAL_MOTOR_CONTROLLER          40000

class MotorControllerConfiguration : public DeviceConfiguration {
public:
    uint16_t speedMax; // in rpm
    float torqueMax;	// maximum torque in 1 Nm
    float torqueSlewRate; // for torque mode only: slew rate of torque value, 0=disabled, in 1Nm/sec
    uint16_t speedSlewRate; //  for speed mode only: slew rate of speed value, 0=disabled, in rpm/sec
    uint8_t reversePercent;
    uint16_t regenTaperUpper; //upper limit where regen tapering starts
    uint16_t regenTaperLower; //lower RPM limit below which no regen will happen

    float mphConvFactor;
    uint32_t odometer; //store this in hundredths of a mile

    //well, these might be able to be in the motor controller class. But, really people
    //could have many ways to select a gear - discrete inputs like this, or via a CAN gearbox
    //or via an analog input that detects the position of a shifter. So, really this belongs
    //elsewhere too.
    uint8_t enableIn;
    uint8_t reverseIn;
    uint8_t forwardIn;
};

class MotorController: public Device {

public:
    enum Gears {
        NEUTRAL = 0,
        DRIVE =   10,
        REVERSE = -10,
        ERROR =   0xFF,
    };

    enum PowerMode {
        modeTorque,
        modeSpeed
    };

    enum OperationState {
        DISABLED =  0,
        STANDBY =   1,
        ENABLE =    2,
        POWERDOWN = 3
    };

    union MotorStatus
    {
        uint32_t ready: 1;
        uint32_t running: 1;
        uint32_t warning: 1;
        uint32_t faulted: 1;
        uint32_t oscillationLimiter: 1;
        uint32_t maxModulationLimiter: 1;
        uint32_t overTempCtrl: 1;
        uint32_t overTempMotor: 1;
        uint32_t overSpeed: 1;
        uint32_t hvUnderVoltage: 1;
        uint32_t hvOverVoltage: 1;
        uint32_t hvOverCurrent: 1;
        uint32_t acOverCurrent: 1;
        uint32_t limitTorque: 1;
        uint32_t limitMaxTorque: 1;
        uint32_t limitSpeed: 1;
        uint32_t limitCtrlTemp: 1;
        uint32_t limitMotorTemp: 1;
        uint32_t limitSlewRate: 1;
        uint32_t limitMotorModel: 1;
        uint32_t limitMechPower: 1;
        uint32_t limitACVoltage: 1;
        uint32_t limitDCVoltage: 1;
        uint32_t limitACCurrent: 1;
        uint32_t limitDCCurrent: 1;
        uint32_t bitfield;
    };

    MotorController();
    DeviceType getType();
    void setup();
    void handleTick();
    uint32_t getTickInterval();

    void loadConfiguration();
    void saveConfiguration();

    void coolingcheck();
    void checkBrakeLight();
    void checkReverseLight();
    void checkEnableInput();
    void checkGearInputs();
    
    void brakecheck();
    bool isReady();
    bool isRunning();
    bool isFaulted();
    bool isWarning();

    uint32_t getStatusBitfield();

    MotorStatus statusBitfield; // bitfield variable for use of the specific implementation

    void setPowerMode(PowerMode mode);
    PowerMode getPowerMode();
    void setOpState(OperationState op) ;
    OperationState getOpState() ;
    void setSelectedGear(Gears gear);
    Gears getSelectedGear();

    int16_t getThrottle();
    uint8_t getEnableIn();
    uint8_t getReverseIn();
    uint8_t getForwardIn();
    int16_t getselectedGear();
    int16_t getSpeedRequested();
    int16_t getSpeedActual();
    float getTorqueRequested();
    float getTorqueActual();
    float getTorqueAvailable();
    uint32_t getOdometerReading();
    float getMPH();
    int preMillis();

    float getDcVoltage();
    float getDcCurrent();
    float getAcCurrent();
    float getMechanicalPower();
    float getTemperatureMotor();
    float getTemperatureInverter();
    float getTemperatureSystem();

    int milliseconds;
    int seconds;
    int minutes;
    int hours ;

protected:
    bool ready; // indicates if the controller is ready to enable the power stage
    bool running; // indicates if the power stage of the inverter is operative
    bool faulted; // indicates a error condition is present in the controller
    bool warning; // indicates a warning condition is present in the controller
    bool testenableinput;
    bool testreverseinput;

    OperationState operationState; //the op state we want

    int16_t throttleRequested; // -1000 to 1000 (per mille of throttle level)
    int16_t speedRequested; // in rpm
    int16_t speedActual; // in rpm
    float torqueRequested; // in Nm
    float torqueActual; // in Nm
    float torqueAvailable; // the maximum available torque in Nm

    float dcVoltage; // DC voltage in Volts
    float dcCurrent; // DC current in Amps
    float acCurrent; // AC current in Amps
    float mechanicalPower; // mechanical power of the motor kW
    float temperatureMotor; // temperature of motor in degree C
    float temperatureInverter; // temperature of inverter power stage in degree C
    float temperatureSystem; // temperature of controller in degree C

    uint32_t skipcounter;

private:
    Gears selectedGear;
    char gearText[16];
    PowerMode powerMode;
    uint32_t lastOdoAccum;
    double odo_accum;
};

#endif


