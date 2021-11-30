/*
 * MotorController.cpp
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

#include "MotorController.h"

MotorController::MotorController() : Device() {
    ready = false;
    running = false;
    faulted = false;
    warning = false;

    temperatureMotor = 0;
    temperatureInverter = 0;
    temperatureSystem = 0;

    statusBitfield.bitfield = 0;

    powerMode = modeTorque;
    throttleRequested = 0;
    speedRequested = 0;
    speedActual = 0;
    torqueRequested = 0;
    torqueActual = 10;
    torqueAvailable = 0;
    mechanicalPower = 0;

    selectedGear = NEUTRAL;
    operationState = ENABLE;

    dcVoltage = 0;
    dcCurrent = 0;
    acCurrent = 0;
    nominalVolts = 0;

    coolflag = false;
    skipcounter = 0;
    testenableinput = 0;
    testreverseinput = 0;
    premillis = 0;
}

void MotorController::setup() {

    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();

    cfgEntries.reserve(20);

    ConfigEntry entry;
    entry = {"TORQ", "Set torque upper limit (tenths of a Nm)", &config->torqueMax, CFG_ENTRY_VAR_TYPE::UINT16, 0, 50000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TORQSLEW", "Torque slew rate (per second, tenths of a Nm)", &config->torqueSlewRate, CFG_ENTRY_VAR_TYPE::UINT16, 0, 50000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"RPM", "Set maximum RPM", &config->speedMax, CFG_ENTRY_VAR_TYPE::UINT16, 0, 30000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"RPMSLEW", "RPM Slew rate (per second)", &config->speedSlewRate, CFG_ENTRY_VAR_TYPE::UINT16, 0, 50000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"REVLIM", "How much torque to allow in reverse (Tenths of a percent)", &config->reversePercent, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"ENABLEIN", "Digital input to enable motor controller (0-3, 255 for none)", &config->enableIn, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"REVIN", "Digital input to reverse motor rotation (0-3, 255 for none)", &config->reverseIn, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TAPERHI", "Regen taper upper RPM (0 - 20000)", &config->regenTaperUpper, CFG_ENTRY_VAR_TYPE::UINT16, 0, 20000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TAPERLO", "Regen taper lower RPM (0 - 20000)", &config->regenTaperLower, CFG_ENTRY_VAR_TYPE::UINT16, 0, 20000, 0, nullptr};
    cfgEntries.push_back(entry);

    statusBitfield.bitfield = 0;

    nominalVolts = config->nominalVolt;
    capacity = config->capacity;
    premillis = millis();

    coolflag = false;

    Device::setup();
}


void MotorController::handleTick() {

    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    statusBitfield.ready = ready;
    statusBitfield.running = running;
    statusBitfield.warning = warning;
    statusBitfield.faulted = faulted;

    //Calculate killowatts and kilowatt hours
    mechanicalPower = dcVoltage * dcCurrent / 10000; //In kilowatts. DC voltage is x10

    //Throttle check
    Throttle *accelerator = deviceManager.getAccelerator();
    Throttle *brake = deviceManager.getBrake();
    if (accelerator)
        throttleRequested = accelerator->getLevel();
    if (brake && brake->getLevel() < -10 && brake->getLevel() < accelerator->getLevel()) //if the brake has been pressed it overrides the accelerator.
        throttleRequested = brake->getLevel();
    //Logger::debug("Throttle: %d", throttleRequested);

    if(skipcounter++ > 30)    //A very low priority loop for checks that only need to be done once per second.
    {
        skipcounter=0; //Reset our laptimer

        coolingcheck();
        checkBrakeLight();
        checkEnableInput();
        checkReverseInput();
        checkReverseLight();
    }
}

//This routine is used to set an optional cooling fan output to on if the current temperature
//exceeds a specified value.  Annunciators are set on website to indicate status.
void MotorController::coolingcheck()
{
    int coolfan=getCoolFan();

    if(coolfan>=0 and coolfan<8)    //We have 8 outputs 0-7 If they entered something else, there is no point in doing this check.
    {
        if(temperatureInverter/10>getCoolOn())  //If inverter temperature greater than COOLON, we want to turn on the coolingoutput
        {
            if(!coolflag)
            {
                coolflag=1;
                systemIO.setDigitalOutput(coolfan, 1); //Turn on cooling fan output
                //statusBitfield1 |=1 << coolfan; //set bit to turn on cooling fan output annunciator
                //statusBitfield3 |=1 << 9; //Set bit to turn on OVERTEMP annunciator
            }
        }

        if(temperatureInverter/10<getCoolOff()) //If inverter temperature falls below COOLOFF, we want to turn cooling off.
        {
            if(coolflag)
            {
                coolflag=0;
                systemIO.setDigitalOutput(coolfan, 0); //Set cooling fan output off
                //statusBitfield1 &= ~(1 << coolfan); //clear bit to turn off cooling fan output annunciator
                //statusBitfield3 &= ~(1 << 9); //clear bit to turn off OVERTEMP annunciator
            }
        }
    }
}

//If we have a brakelight output configured, this will set it anytime regen greater than 10 Newton meters
void MotorController::checkBrakeLight()
{

    if(getBrakeLight() >=0 && getBrakeLight()<8)  //If we have one configured ie NOT 255 but a valid output
    {
        int brakelight=getBrakeLight();  //Get brakelight output once

        if(getTorqueActual() < -100)  //We only want to turn on brake light if we are have regen of more than 10 newton meters
        {
            systemIO.setDigitalOutput(brakelight, 1); //Turn on brake light output
            //statusBitfield1 |=1 << brakelight; //set bit to turn on brake light output annunciator
        }
        else
        {
            systemIO.setDigitalOutput(brakelight, 0); //Turn off brake light output
            //statusBitfield1 &= ~(1 << brakelight);//clear bit to turn off brake light output annunciator
        }
    }

}

//If a reverse light output is configured, this will turn it on anytime the gear state is in REVERSE
void MotorController::checkReverseLight()
{
    uint16_t reverseLight=getRevLight();
    if(reverseLight >=0 && reverseLight <8) //255 means none selected.  We don't have a reverselight output configured.
    {
        if(selectedGear==REVERSE)  //If the selected gear IS reverse
        {
            systemIO.setDigitalOutput(reverseLight, true); //Turn on reverse light output
            //statusBitfield1 |=1 << reverseLight; //set bit to turn on reverse light output annunciator
        }
        else
        {
            systemIO.setDigitalOutput(reverseLight, false); //Turn off reverse light output
            //statusBitfield1 &= ~(1 << reverseLight);//clear bit to turn off reverselight OUTPUT annunciator
        }
    }
}

//If we have an ENABLE input configured, this will set opstation to ENABLE anytime it is true (12v), DISABLED if not.
void MotorController:: checkEnableInput()
{
    uint16_t enableinput=getEnableIn();
    if(enableinput >= 0 && enableinput<4) //Do we even have an enable input configured ie NOT 255.
    {
        if((systemIO.getDigitalIn(enableinput))||testenableinput) //If it's ON let's set our opstate to ENABLE
        {
            setOpState(ENABLE);
            //statusBitfield2 |=1 << enableinput; //set bit to turn on ENABLE annunciator
            //statusBitfield2 |=1 << 18;//set bit to turn on enable input annunciator
        }
        else
        {
            setOpState(DISABLED);//If it's off, lets set DISABLED.  These two could just as easily be reversed
            //statusBitfield2 &= ~(1 << 18); //clear bit to turn off ENABLE annunciator
            //statusBitfield2 &= ~(1 << enableinput);//clear bit to turn off enable input annunciator
        }
    }
}

//IF we have a reverse input configured, this will set our selected gear to REVERSE any time the input is true, DRIVE if not
void MotorController:: checkReverseInput()
{
    uint16_t reverseinput=getReverseIn();
    if(reverseinput >= 0 && reverseinput<4)  //If we don't have a Reverse Input, do nothing
    {
        if((systemIO.getDigitalIn(reverseinput))||testreverseinput)
        {
            setSelectedGear(REVERSE);
            //statusBitfield2 |=1 << 16; //set bit to turn on REVERSE annunciator
            //statusBitfield2 |=1 << reverseinput;//setbit to Turn on reverse input annunciator
        }
        else
        {
            setSelectedGear(DRIVE); //If it's off, lets set to DRIVE.
            //statusBitfield2 &= ~(1 << 16); //clear bit to turn off REVERSE annunciator
            //statusBitfield2 &= ~(1 << reverseinput);//clear bit to turn off reverse input annunciator
        }
    }
}



bool MotorController::isRunning() {
    return running;
}

bool MotorController::isFaulted() {
    return faulted;
}

bool MotorController::isWarning() {
    return warning;
}


DeviceType MotorController::getType() {
    return (DEVICE_MOTORCTRL);
}
void MotorController::setOpState(OperationState op) {
    operationState = op;
}

MotorController::OperationState MotorController::getOpState() {
    return operationState;
}
MotorController::PowerMode MotorController::getPowerMode() {
    return powerMode;
}

void MotorController::setPowerMode(PowerMode mode) {
    powerMode = mode;
}

uint32_t MotorController::getStatusBitfield() {
    return statusBitfield.bitfield;
}

int8_t MotorController::getCoolFan() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    return config->coolFan;
}
int8_t MotorController::getCoolOn() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    return config->coolOn;
}

int8_t MotorController::getCoolOff() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    return config->coolOff;
}
int8_t MotorController::getBrakeLight() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    return config->brakeLight;
}
int8_t MotorController::getRevLight() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    return config->revLight;
}
int8_t MotorController::getEnableIn() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    return config->enableIn;
}
int8_t MotorController::getReverseIn() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    return config->reverseIn;
}

int16_t MotorController::getThrottle() {
    return throttleRequested;
}

int16_t MotorController::getSpeedRequested() {
    return speedRequested;
}

int16_t MotorController::getSpeedActual() {
    return speedActual;
}

int16_t MotorController::getTorqueRequested() {
    return torqueRequested;
}

int16_t MotorController::getTorqueActual() {
    return torqueActual;
}

MotorController::Gears MotorController::getSelectedGear() {
    return selectedGear;
}
void MotorController::setSelectedGear(Gears gear) {
    selectedGear=gear;
}


int16_t MotorController::getTorqueAvailable() {
    return torqueAvailable;
}

uint16_t MotorController::getDcVoltage() {
    return dcVoltage;
}

int16_t MotorController::getDcCurrent() {
    return dcCurrent;
}

uint16_t MotorController::getAcCurrent() {
    return acCurrent;
}

int16_t MotorController::getnominalVolt() {
    return nominalVolts;
}

int16_t MotorController::getMechanicalPower() {
    return mechanicalPower;
}

int16_t MotorController::getTemperatureMotor() {
    return temperatureMotor;
}

int16_t MotorController::getTemperatureInverter() {
    return temperatureInverter;
}

int16_t MotorController::getTemperatureSystem() {
    return temperatureSystem;
}

uint32_t MotorController::getTickInterval() {
    return CFG_TICK_INTERVAL_MOTOR_CONTROLLER;
}

bool MotorController::isReady() {
    return false;
}

void MotorController::loadConfiguration() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();

    Device::loadConfiguration(); // call parent

    //if (prefsHandler->checksumValid()) { //checksum is good, read in the values stored in EEPROM
        Logger::info((char *)Constants::validChecksum);
        prefsHandler->read("MaxRPM", &config->speedMax, 6000);
        prefsHandler->read("MaxTorque", &config->torqueMax, 3000);
        prefsHandler->read("RPMSlew", &config->speedSlewRate, 10000);
        prefsHandler->read("TorqueSlew", &config->torqueSlewRate, 6000);
        prefsHandler->read("ReversePercentage", &config->reversePercent, 50);
        prefsHandler->read("NominalVoltage", &config->nominalVolt, 3300);
        prefsHandler->read("CoolFanOutput", &config->coolFan, 6);
        prefsHandler->read("CoolOnTemp", &config->coolOn, 40);
        prefsHandler->read("CoolOffTemp", &config->coolOff, 35);
        prefsHandler->read("BrakeLightOutput", &config->brakeLight, 255);
        prefsHandler->read("ReverseLightOutput", &config->revLight, 255);
        prefsHandler->read("Enable_DIN", &config->enableIn, 0);
        prefsHandler->read("Reverse_DIN", &config->reverseIn, 1);
        prefsHandler->read("RegenTaperUpper", &config->regenTaperUpper, 500);
        prefsHandler->read("RegenTaperLower", &config->regenTaperLower, 75);
        if (config->regenTaperLower < 0 || config->regenTaperLower > 10000 ||
            config->regenTaperUpper < config->regenTaperLower || config->regenTaperUpper > 10000) {
            config->regenTaperLower = 75;
            config->regenTaperUpper = 500;
        }
    //DeviceManager::getInstance()->sendMessage(DEVICE_WIFI, ICHIP2128, MSG_CONFIG_CHANGE, NULL);
    Logger::info("MaxTorque: %i MaxRPM: %i", config->torqueMax, config->speedMax);
}

void MotorController::saveConfiguration() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();

    Device::saveConfiguration(); // call parent

    prefsHandler->write("MaxRPM", config->speedMax);
    prefsHandler->write("MaxTorque", config->torqueMax);
    prefsHandler->write("RPMSlew", config->speedSlewRate);
    prefsHandler->write("TorqueSlew", config->torqueSlewRate);
    prefsHandler->write("ReversePercentage", config->reversePercent);
    prefsHandler->write("NominalVoltage", config->nominalVolt);
    prefsHandler->write("CoolFanOutput", config->coolFan);
    prefsHandler->write("CoolOnTemp", config->coolOn);
    prefsHandler->write("CoolOffTemp", config->coolOff);
    prefsHandler->write("BrakeLightOutput", config->brakeLight);
    prefsHandler->write("ReverseLightOutput", config->revLight);
    prefsHandler->write("Enable_DIN", config->enableIn);
    prefsHandler->write("Reverse_DIN", config->reverseIn);
    prefsHandler->write("RegenTaperLower", config->regenTaperLower);
    prefsHandler->write("RegenTaperUpper", config->regenTaperUpper);
    
    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
    loadConfiguration();
}


