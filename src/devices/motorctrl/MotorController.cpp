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

    temperatureMotor = 20.0f;
    temperatureInverter = 20.0f;
    temperatureSystem = 20.0f;

    statusBitfield.bitfield = 0;

    powerMode = modeTorque;
    throttleRequested = 0;
    speedRequested = 0;
    speedActual = 0;
    torqueRequested = 0;
    torqueActual = 0;
    torqueAvailable = 0;
    mechanicalPower = 0;

    selectedGear = NEUTRAL;
    strcpy(gearText, "Neutral");
    operationState = ENABLE;

    dcVoltage = 0;
    dcCurrent = 0;
    acCurrent = 0;

    skipcounter = 0;
    testenableinput = 0;
    testreverseinput = 0;
    odoReadingAtLastSave = 0;
    lastOdoSave = 0;
    lastOdoAccum = 0;
    odo_accum = 0;
    slewedTorque = 0.0;

    deviceType = DEVICE_MOTORCTRL;
}

FLASHMEM void MotorController::setup() {

    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();

    cfgEntries.reserve(20);

    ConfigEntry entry;
    entry = {"TORQ", "Set torque upper limit (Nm)", &config->torqueMax, CFG_ENTRY_VAR_TYPE::FLOAT, {.floating = 0.0}, {.floating = 5000.0}, 1, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TORQSLEW", "Torque slew rate (per second, Nm)", &config->torqueSlewRate, CFG_ENTRY_VAR_TYPE::FLOAT, {.floating = 0.0}, {.floating = 50000.0}, 1, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"RPM", "Set maximum RPM", &config->speedMax, CFG_ENTRY_VAR_TYPE::UINT16, 0, 30000, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"RPMSLEW", "RPM Slew rate (per second)", &config->speedSlewRate, CFG_ENTRY_VAR_TYPE::UINT16, 0, 50000, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"REVLIM", "How much torque to allow in reverse (Tenths of a percent)", &config->reversePercent, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"ENABLEIN", "Digital input to enable motor controller (0-11, 255 for none)", &config->enableIn, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"FWDIN", "Digital input to enable forward motion (0-11, 255 for none)", &config->forwardIn, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"REVIN", "Digital input to enable reverse motion (0-11, 255 for none)", &config->reverseIn, CFG_ENTRY_VAR_TYPE::BYTE, 0, 255, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TAPERHI", "Regen taper upper RPM (0 - 20000)", &config->regenTaperUpper, CFG_ENTRY_VAR_TYPE::UINT16, 0, 20000, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TAPERLO", "Regen taper lower RPM (0 - 20000)", &config->regenTaperLower, CFG_ENTRY_VAR_TYPE::UINT16, 0, 20000, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"MPHFACTOR", "Set factor to multiply RPM by to get MPH", &config->mphConvFactor, CFG_ENTRY_VAR_TYPE::FLOAT, {.floating = 0.0}, {.floating = 1.0}, 4, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"ODO-READING", "How many miles should be on the odometer? (In hundredths of a mile)", &config->odometer, CFG_ENTRY_VAR_TYPE::UINT32, 0, 100000000ul, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    
    statusBitfield.bitfield = 0;

    StatusEntry stat;
    //        name       var         type              prevVal  obj
    stat = {"MC_Ready", &ready, CFG_ENTRY_VAR_TYPE::BYTE, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_Running", &running, CFG_ENTRY_VAR_TYPE::BYTE, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_Faulted", &faulted, CFG_ENTRY_VAR_TYPE::BYTE, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_Warning", &warning, CFG_ENTRY_VAR_TYPE::BYTE, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_Gear", &selectedGear, CFG_ENTRY_VAR_TYPE::INT16, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_PowerMode", &powerMode, CFG_ENTRY_VAR_TYPE::BYTE, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_OpState", &operationState, CFG_ENTRY_VAR_TYPE::BYTE, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_ThrottleReq", &throttleRequested, CFG_ENTRY_VAR_TYPE::INT16, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_SpeedReq", &speedRequested, CFG_ENTRY_VAR_TYPE::INT16, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_SpeedAct", &speedActual, CFG_ENTRY_VAR_TYPE::INT16, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_TorqueReq", &torqueRequested, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_TorqueAct", &torqueActual, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_TorqueMax", &torqueAvailable, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_DCVoltage", &dcVoltage, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_DCCurrent", &dcCurrent, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_ACCurrent", &acCurrent, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_MechPower", &mechanicalPower, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_MotorTemp", &temperatureMotor, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_InverterTemp", &temperatureInverter, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_SysTemp", &temperatureSystem, CFG_ENTRY_VAR_TYPE::FLOAT, 0, this};
    deviceManager.addStatusEntry(stat);
    
    Device::setup();
}


void MotorController::handleTick() {

    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    statusBitfield.ready = ready;
    statusBitfield.running = running;
    statusBitfield.warning = warning;
    statusBitfield.faulted = faulted;

    //Calculate killowatts and kilowatt hours
    mechanicalPower = dcVoltage * dcCurrent / 1000.0f; //In kilowatts.

    //do odometer calculations
    if (lastOdoAccum == 0)
    {
        lastOdoAccum = micros();
    }
    else
    {
        //distance traveled is MPH 
        //1MPH * 1 hr = 1 Mile so here we've got MPH and time which will be MUCH smaller than an hour
        //the time since last tick is (micros() - lastAccum) / a million in seconds. 1 second is 
        //1 / 3600th of an hour
        //so, take MPH and multiply by (interval in microseconds / 3.6 billion)
        uint32_t timestamp = micros();
        uint32_t interval = timestamp - lastOdoAccum;
        lastOdoAccum = timestamp;

        //speed is in RPM which is per minute while we want miles per hour. 
        //But, miles are long. So, the conversion factor is likely to need to be pretty low.
        double mph = abs(getSpeedActual()) * config->mphConvFactor;

        odo_accum += (mph * interval) / 3600000000.0;
        //now, the odometer in config is in hundedths of a mile so keep adding those as much as possible
        //the while is almost superfluous as it is essentially physically impossible to gain 0.01 miles
        //in the tick interval. So, this could only happen if it missed a couple of ticks and you were
        //going WAY too fast.
        while (odo_accum > 0.01)
        {
            config->odometer++;
            odo_accum -= 0.01;
        }
    }

    //now, save the odometer reading every so often if it has changed
    if (config->odometer > odoReadingAtLastSave)
    {
        if ((millis() - lastOdoSave) >= 60000) //save every minute 
        {
            lastOdoSave = millis();
            prefsHandler->write("odometer", config->odometer);
            prefsHandler->forceCacheWrite();
            odoReadingAtLastSave = config->odometer;
        }
    }

    //Throttle check
    Throttle *accelerator = deviceManager.getAccelerator();
    Throttle *brake = deviceManager.getBrake();
    if (accelerator)
        throttleRequested = accelerator->getLevel();
    if (brake && brake->getLevel() < -10 && brake->getLevel() < accelerator->getLevel()) //if the brake has been pressed it overrides the accelerator.
        throttleRequested = brake->getLevel();
    //Logger::debug("Throttle: %d", throttleRequested);

    ChargeController *charger = static_cast<ChargeController *>(deviceManager.getDeviceByType(DeviceType::DEVICE_CHARGER));
    if (charger && charger->getEVSEConnected()) throttleRequested = 0; //NO DRIVING AWAY!

    if (skipcounter++ > 30)    //A very low priority loop for checks that only need to be done once per second.
    {
        skipcounter = 0; //Reset our laptimer
        checkEnableInput();
        checkGearInputs();
    }
}

float MotorController::getSlewedTorque()
{
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    //if we're asking for regen but are going slow or backward of the motoring direction then
    //zero out the request for regen. Otherwise, allow regen
    //if ((torqueRequested < 0) && (speedActual < 200)) torqueRequested = 0;

    //now, take torqueRequested and compare it to torqueCommand. If it is farther away than our slew rate
    //then just move toward target by slew rate. Otherwise set it directly
    float slewInc = config->torqueSlewRate / (1000000.0f / getTickInterval());
    Logger::debug("slewInc %f  torqueTarget %f", slewInc, torqueRequested);
    if (slewInc < 0.05f) slewInc = 0.05f; //just for sanity. Even a stupidly low value set to torqueSlewRate will still do... something.
    if (torqueRequested > 0)
    {
        if (torqueRequested > slewedTorque) 
        {
            slewedTorque += slewInc;
            if (slewedTorque > torqueRequested) slewedTorque = torqueRequested;
            Logger::debug("Going up %f", slewedTorque);
        }
        else 
        {
            slewedTorque -= (slewInc * 5.0f);
            if (slewedTorque < torqueRequested) slewedTorque = torqueRequested;
        }
    }
    else if (torqueRequested <= 0)
    {
        if (torqueRequested < slewedTorque)
        {
            slewedTorque -= slewInc;
            if (slewedTorque < torqueRequested) slewedTorque = torqueRequested;
        }
        else 
        {
            slewedTorque += (slewInc * 5.0f);
            if (slewedTorque > torqueRequested) slewedTorque = torqueRequested;
        }
    }

    //if (torqueRequested == 0) slewedTorque = 0;

    return slewedTorque;
}

float MotorController::getMPH()
{
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();

    return abs(getSpeedActual()) * config->mphConvFactor;
}

/*
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
*/

//If we have an ENABLE input configured, this will set opstation to ENABLE anytime it is true (12v), DISABLED if not.
void MotorController:: checkEnableInput()
{
    uint16_t enableinput = getEnableIn();
    if(enableinput >= 0 && enableinput < 255) //Do we even have an enable input configured ie NOT 255.
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
            setSelectedGear(NEUTRAL);
            //statusBitfield2 &= ~(1 << 18); //clear bit to turn off ENABLE annunciator
            //statusBitfield2 &= ~(1 << enableinput);//clear bit to turn off enable input annunciator
        }
    }
    if (enableinput == 255) setOpState(ENABLE);
}

//IF we have a reverse input configured, this will set our selected gear to REVERSE any time the input is true, DRIVE if not
void MotorController::checkGearInputs()
{
    if (getOpState() != ENABLE) return;
    uint8_t reverseinput = getReverseIn();
    uint8_t forwardInput = getForwardIn();
    Gears selGear = NEUTRAL;
    if(reverseinput >= 0 && reverseinput < 255)  //If we don't have a Reverse Input, do nothing
    {
        if((systemIO.getDigitalIn(reverseinput)) || testreverseinput)
        {
            selGear = REVERSE;
            //statusBitfield2 |=1 << 16; //set bit to turn on REVERSE annunciator
            //statusBitfield2 |=1 << reverseinput;//setbit to Turn on reverse input annunciator
        }
    }

    if(forwardInput >= 0 && forwardInput < 255)  //If we don't have a Reverse Input, do nothing
    {
        if((systemIO.getDigitalIn(forwardInput)) )
        {
            selGear = DRIVE;
            //statusBitfield2 |=1 << 16; //set bit to turn on REVERSE annunciator
            //statusBitfield2 |=1 << reverseinput;//setbit to Turn on reverse input annunciator
        }
    }
    //lastly, if we're still in neutral but there was a reverse input and there isn't a forward input
    //then the correct thing to do is go into forward mode.
    if ( (selGear == NEUTRAL) && 
         (reverseinput < 255) && 
         (forwardInput == 255) ) selGear = DRIVE;

    Logger::debug("Selected gear: %i", selGear);

    setSelectedGear(selGear);
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

uint8_t MotorController::getEnableIn() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    return config->enableIn;
}

uint8_t MotorController::getReverseIn() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    return config->reverseIn;
}

uint8_t MotorController::getForwardIn() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    return config->forwardIn;
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

float MotorController::getTorqueRequested() {
    return torqueRequested;
}

float MotorController::getTorqueActual() {
    return torqueActual;
}

MotorController::Gears MotorController::getSelectedGear() {
    return selectedGear;
}
void MotorController::setSelectedGear(Gears gear) {
    selectedGear = gear;
    switch (gear)
    {
    case NEUTRAL:
        strcpy(gearText, "Neutral");
        break;
    case DRIVE:
        strcpy(gearText, "Drive");
        break;
    case REVERSE:
        strcpy(gearText, "Reverse");
        break;
    default:
        strcpy(gearText, "ERROR");
        break;
    }
}

float MotorController::getTorqueAvailable() {
    return torqueAvailable;
}

float MotorController::getDcVoltage() {
    return dcVoltage;
}

float MotorController::getDcCurrent() {
    return dcCurrent;
}

float MotorController::getAcCurrent() {
    return acCurrent;
}

float MotorController::getMechanicalPower() {
    return mechanicalPower;
}

float MotorController::getTemperatureMotor() {
    return temperatureMotor;
}

float MotorController::getTemperatureInverter() {
    return temperatureInverter;
}

float MotorController::getTemperatureSystem() {
    return temperatureSystem;
}

uint32_t MotorController::getTickInterval() {
    return CFG_TICK_INTERVAL_MOTOR_CONTROLLER;
}

bool MotorController::isReady() {
    return false;
}

const char* MotorController::getFaultDescription(uint16_t faultcode)
{
    if ((faultcode >= 1000) && (faultcode < MCTRL_LAST_FAULT) ) return MCTRL_FAULT_DESCS[faultcode-1000];
    return Device::getFaultDescription(faultcode); //try generic device class if we couldn't handle it
    return nullptr; //no match, return nothing
}


uint32_t MotorController::getOdometerReading()
{
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();
    return config->odometer;
}

void MotorController::loadConfiguration() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();

    Device::loadConfiguration(); // call parent

    //if (prefsHandler->checksumValid()) { //checksum is good, read in the values stored in EEPROM
        Logger::info((char *)Constants::validChecksum);
        prefsHandler->read("MaxRPM", &config->speedMax, 6000);
        prefsHandler->read("MaxTorque", &config->torqueMax, 300.0f);
        prefsHandler->read("RPMSlew", &config->speedSlewRate, 10000);
        prefsHandler->read("TorqueSlew", &config->torqueSlewRate, 600.0f);
        prefsHandler->read("ReversePercentage", &config->reversePercent, 50);
        prefsHandler->read("Enable_DIN", &config->enableIn, 0);
        prefsHandler->read("Reverse_DIN", &config->reverseIn, 1);
        prefsHandler->read("RegenTaperUpper", &config->regenTaperUpper, 500);
        prefsHandler->read("RegenTaperLower", &config->regenTaperLower, 75);
        prefsHandler->read("FwdDIN", &config->forwardIn, 255);
        prefsHandler->read("MPHFactor", &config->mphConvFactor, 0.5f);
        prefsHandler->read("odometer", &config->odometer, 0);
        odoReadingAtLastSave = config->odometer;
        lastOdoSave = millis();
        if (config->regenTaperLower < 0 || config->regenTaperLower > 10000 ||
            config->regenTaperUpper < config->regenTaperLower || config->regenTaperUpper > 10000) {
            config->regenTaperLower = 75;
            config->regenTaperUpper = 500;
        }
    //DeviceManager::getInstance()->sendMessage(DEVICE_WIFI, ICHIP2128, MSG_CONFIG_CHANGE, NULL);
    Logger::info("MaxTorque: %.1f MaxRPM: %i", config->torqueMax, config->speedMax);
}

void MotorController::saveConfiguration() {
    MotorControllerConfiguration *config = (MotorControllerConfiguration *)getConfiguration();

    Device::saveConfiguration(); // call parent

    prefsHandler->write("MaxRPM", config->speedMax);
    prefsHandler->write("MaxTorque", config->torqueMax);
    prefsHandler->write("RPMSlew", config->speedSlewRate);
    prefsHandler->write("TorqueSlew", config->torqueSlewRate);
    prefsHandler->write("ReversePercentage", config->reversePercent);
    prefsHandler->write("Enable_DIN", config->enableIn);
    prefsHandler->write("Reverse_DIN", config->reverseIn);
    prefsHandler->write("FwdDIN", config->forwardIn);
    prefsHandler->write("RegenTaperLower", config->regenTaperLower);
    prefsHandler->write("RegenTaperUpper", config->regenTaperUpper);
    prefsHandler->write("MPHFactor", config->mphConvFactor);
    prefsHandler->write("odometer", config->odometer);

    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
    //loadConfiguration();
}


