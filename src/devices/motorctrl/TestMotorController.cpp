/*
 * TestMotorController.cpp
 *
 * Fake motor controller that takes throttle input and pretends like it's driving a motor.
 * Used just for testing surrounding code (throttle input, status output, etc)
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

#include "TestMotorController.h"

TestMotorController::TestMotorController() : MotorController() {    
    selectedGear = DRIVE;
    commonName = "Test Inverter";
}

void TestMotorController::earlyInit()
{
    prefsHandler = new PrefHandler(TESTINVERTER);
}

void TestMotorController::setup() {
    tickHandler.detach(this);

    Logger::info("add device: Test Inverter (id:%X, %X)", TESTINVERTER, this);

    loadConfiguration();
    MotorController::setup(); // run the parent class version of this function

    running = true;
    setPowerMode(modeTorque);
    setSelectedGear(DRIVE);
    setOpState(ENABLE);

    tickHandler.attach(this, CFG_TICK_INTERVAL_MOTOR_CONTROLLER_DMOC);
}

void TestMotorController::handleTick() {
    TestMotorControllerConfiguration *config = (TestMotorControllerConfiguration *)getConfiguration();
    
    MotorController::handleTick(); //kick the ball up to papa
    
    dcVoltage = 3600; //360v nominal voltage scaled up 10x

    //use throttleRequested to produce some motor like output.
    //throttleRequested ranges +/- 1000 so regen is possible here if the throttle has it set up.
    
    if (powerMode == modeSpeed)
    {
        torqueRequested = 0;
        if (throttleRequested > 0 && operationState == ENABLE && selectedGear != NEUTRAL)
            speedRequested = (((long) throttleRequested * (long) config->speedMax) / 1000);
        else
            speedRequested = 0;
        
        speedActual = ((speedActual * 8) + (speedRequested * 2)) / 10;
        torqueActual = speedActual / 20;
        
        //generate some baseline holding current to maintain the speed.
        dcCurrent = speedRequested / 66;
        //Then add some accelerating current for the difference between target and actual
        dcCurrent += (speedRequested - speedActual) / 10;
    }
    else
    {
        if (selectedGear == DRIVE)
            torqueRequested = (((long) throttleRequested * (long) config->torqueMax) / 1000L);
        if (selectedGear == REVERSE)
            torqueRequested = (((long) throttleRequested * -1 *(long) config->torqueMax) / 1000L);
        
        torqueActual = ((torqueActual * 7) + (torqueRequested * 3)) / 10;
        speedActual = torqueActual * 2;
        if (speedActual < 0) speedActual = 0;
        
        speedRequested = 0;
        
        //generate some baseline holding current to maintain the speed.
        dcCurrent = torqueRequested / 3;
        //Then add some accelerating current for the difference between target and actual
        dcCurrent += (torqueRequested - torqueActual) * 2;        
                
    }
    
    acCurrent = (dcCurrent * 40) / 30; //A bit more current than DC bus    
    
    //Both dc current and dc voltage are scaled up 10, mech power should be in 0.1kw increments
    //current*voltage = watts but there is inefficiency to deal with. and it's watts but we're scaled up 100x
    //because of multipliers so need to scale down 100x to get to watts then by 100x again to get to 0.1kw
    //100x100 = 10000 but inefficiency should make it even worse
    mechanicalPower = (dcCurrent * dcVoltage) / 12000;
    
    //Heat up or cool motor and inverter based on mechanical power being used.
    //Assume ambient temperature is 18C
    //These numbers are horrifically off from realistic physics at this point
    //but we're trying to aid debugging, not making a perfect physics model.
    temperatureMotor = 180 + abs(mechanicalPower * 2);
    temperatureInverter = 190 + abs(mechanicalPower * 3) / 2;
    temperatureSystem = (temperatureInverter + temperatureMotor) / 2;
    
    Logger::debug(TESTINVERTER, "PowerMode: %i, Gear: %i", powerMode, selectedGear);
    Logger::debug(TESTINVERTER, "TorqueReq: %i, SpeedReq: %i", torqueRequested, speedRequested);
    Logger::debug(TESTINVERTER, "dcCurrent: %i, mechPower: %i", dcCurrent, mechanicalPower);
    
}

void TestMotorController::setGear(Gears gear) {
    selectedGear = gear;
    //if the gear was just set to drive or reverse and the DMOC is not currently in enabled
    //op state then ask for it by name
    if (selectedGear != NEUTRAL) {
        operationState = ENABLE;
    }
    //should it be set to standby when selecting neutral? I don't know. Doing that prevents regen
    //when in neutral and I don't think people will like that.
}

DeviceId TestMotorController::getId() {
    return (TESTINVERTER);
}

uint32_t TestMotorController::getTickInterval()
{
    return CFG_TICK_INTERVAL_MOTOR_CONTROLLER_DMOC;
}

void TestMotorController::loadConfiguration() {
    TestMotorControllerConfiguration *config = (TestMotorControllerConfiguration *)getConfiguration();

    if (!config) {
        config = new TestMotorControllerConfiguration();
        setConfiguration(config);
    }

    MotorController::loadConfiguration(); // call parent
}

void TestMotorController::saveConfiguration() {
    MotorController::saveConfiguration();
}

TestMotorController testMC;