/*
 * LeafMotorController.cpp
 *
 * Heavy amounts of peeking were done into the STM32-VCU code
 * found here: https://github.com/damienmaguire/Stm32-vcu/blob/master/src/leafinv.cpp
 * All credit for the actual implementation details goes to Damien Maguire, et al.
 * 
 Copyright (c) 2022 Collin Kidder

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

#include "LeafMotorController.h"

extern bool runThrottle; //TODO: remove use of global variables !

LeafMotorController::LeafMotorController() : MotorController() {    
    operationState = DISABLED;
    actualState = DISABLED;
    commonName = "Leaf Inverter";
    shortName = "LEAFINV";
    deviceId = LEAFINV;
}

void LeafMotorController::setup() {
    tickHandler.detach(this);

    Logger::info("add device: LeafInverter (id:%X, %X)", LEAFINV, this);

    loadConfiguration();
    MotorController::setup(); // run the parent class version of this function

    ConfigEntry entry;
    //        cfgName          helpText                               variable ref        Type                   Min Max Precision Funct
    entry = {"LEAFINV-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);

    StatusEntry stat;
    //        name       var         type              prevVal  obj
    stat = {"MC_ActualState", &actualState, CFG_ENTRY_VAR_TYPE::BYTE, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_Alive", &alive, CFG_ENTRY_VAR_TYPE::BYTE, 0, this};
    deviceManager.addStatusEntry(stat);
    stat = {"MC_torqueCmd", &torqueCommand, CFG_ENTRY_VAR_TYPE::UINT16, 0, this};
    deviceManager.addStatusEntry(stat);

    setAttachedCANBus(config->canbusNum);

    // register ourselves as observer of 0x23x and 0x65x can frames
    //attachedCANBus->attach(this, 0x230, 0x7f0, false);
    //attachedCANBus->attach(this, 0x650, 0x7f0, false);

    running = false;
    setPowerMode(modeTorque);
    setSelectedGear(NEUTRAL);
    setOpState(DISABLED );
    ms=millis();
    setAlive();

    tickHandler.attach(this, CFG_TICK_INTERVAL_MOTOR_CONTROLLER_LEAF);
}

/*
 Finally, the firmware actually processes some of the status messages from the DmocMotorController
 However, currently the alive and checksum bytes aren't checked for validity.
 To be valid a frame has to have a different alive value than the last value we saw
 and also the checksum must match the one we calculate. Right now we'll just assume
 everything has gone according to plan.
 */

void LeafMotorController::handleCanFrame(const CAN_message_t &frame) {
    uint16_t speedTemp;

    setAlive();

    Logger::debug(LEAFINV, "CAN received: %X  %X  %X  %X  %X  %X  %X  %X  %X", frame.id,frame.buf[0] ,frame.buf[1],frame.buf[2],frame.buf[3],frame.buf[4],frame.buf[5],frame.buf[6],frame.buf[7]);


    switch (frame.id)
    {
    case 0x1DA:
        dcVoltage = ((frame.buf[0] << 2) + (frame.buf[1] >> 6));
        speedTemp = (frame.buf[4] << 8) + frame.buf[5];
        speedActual = (speedTemp == 0x7FFF ? 0 : speedTemp);
        if ((frame.buf[6] & 0xB0) != 0) faulted = true;
        break;
    case 0x55A: //temperatures
        temperatureInverter = (frame.buf[2] - 32) * 0.555555; //stored in Fahrenheit?! Why?!
        temperatureMotor = (frame.buf[1] - 32) * 0.555555;
        break;
    }
}

void LeafMotorController::handleTick() {

    MotorController::handleTick(); //kick the ball up to papa

    checkAlive(1000);

    if (isOperational)
    {
        {
            //Logger::debug(LEAFINV, "Enable Input Active? %u         Reverse Input Active? %u" ,systemIO.getDigitalIn(getEnableIn()),systemIO.getDigitalIn(getReverseIn()));
            //if(getEnableIn() < 0) setOpState(ENABLE); //If we HAVE an enableinput 0-3, we'll let that handle opstate. Otherwise set it to ENABLE
            //if(getReverseIn() < 0) setSelectedGear(DRIVE); //If we HAVE a reverse input, we'll let that determine forward/reverse.  Otherwise set it to DRIVE
        }
        running = true;
    }
    else {
        running = false;
        setSelectedGear(NEUTRAL); //We will stay in NEUTRAL until we get at least 40 frames ahead indicating continous communications.
    }

    //sendCmd1();  //This actually sets our GEAR and our actualstate cycle
}

//gear selection, car on/off
void LeafMotorController::sendFrame11A()
{
    static int counter = 0;
    CAN_message_t output;
    Gears currentGear = getSelectedGear();
    output.len = 8;
    output.id = 0x11A;
    output.flags.extended = 0; //standard frame

    //byte 1 sets the gear in the upper nibble. No idea what lower nibble might be, try ignoring it
    //values for gear = 2 "Reverse" 3 "Neutral" 4 "Drive/B" 0 "Parked" 
    output.buf[0] = 0x00;
    if (operationState == ENABLE)
    {
        if (currentGear == DRIVE) output.buf[0] = 4 << 4;
        else if (currentGear == REVERSE) output.buf[0] = 2 << 4;
    }

    if (operationState != ENABLE) output.buf[1] = 0x80;
    else output.buf[1] = 0x40;

    output.buf[2] = 0; //steering wheel buttons. We ain't pushing any
    output.buf[3] = 0xAA; //heartbeat vcm but we're not using it I guess
    output.buf[4] = 0xC0; //no idea.
    output.buf[5] = 0; //seems not to be used
    counter = (counter + 1) & 3;
    output.buf[6] = (uint8_t) counter;

    calcChecksum(output); //fills out byte 7
 
    Logger::debug(LEAFINV, "0x11A tx: %X %X %X %X %X %X %X %X", output.buf[0], output.buf[1], output.buf[2], output.buf[3],
                  output.buf[4], output.buf[5], output.buf[6], output.buf[7]);

    attachedCANBus->sendFrame(output);

}

//torque request, charge status
void LeafMotorController::sendFrame1D4()
{

}

//battery status msg 1
void LeafMotorController::sendFrame1DB()
{

}

//battery status msg 2
void LeafMotorController::sendFrame1DC()
{

}

//charger cmd from vcu
void LeafMotorController::sendFrame1F2()
{

}

//vcu sends, hcm wake up. Do we need it?
void LeafMotorController::sendFrame50B()
{

}

//battery msg, SOC, misc stuff
void LeafMotorController::sendFrame55B()
{

}

//fast charge stuff for bms
void LeafMotorController::sendFrame59E()
{

}

//bms - remaining charge, output limit reason, temperature to dash
void LeafMotorController::sendFrame5BC()
{

}

void LeafMotorController::taperRegen()
{
    LeafMotorControllerConfiguration *config = (LeafMotorControllerConfiguration *)getConfiguration();
    if (speedActual < config->regenTaperLower) torqueRequested = 0;
    else {        
        int32_t range = config->regenTaperUpper - config->regenTaperLower; //next phase is to not hard code this
        int32_t taper = speedActual - config->regenTaperLower;
        int32_t calc = (torqueRequested * taper) / range;
        torqueRequested = (int16_t)calc;
    }
}

void LeafMotorController::setGear(Gears gear) {
    setSelectedGear(gear);
    //if the gear was just set to drive or reverse and the DMOC is not currently in enabled
    //op state then ask for it by name
    if (gear != NEUTRAL) {
        operationState = ENABLE;
    }
    //should it be set to standby when selecting neutral? I don't know. Doing that prevents regen
    //when in neutral and I don't think people will like that.
}

byte LeafMotorController::calcChecksum(CAN_message_t &thisFrame) {
    uint8_t poly = 0x85;
    thisFrame.buf[7] = 0;
    uint8_t crc = 0;
    for(int b=0; b<8; b++)
    {
        for(int i=7; i>=0; i--)
        {
            uint8_t bit = ((thisFrame.buf[b] &(1 << i)) > 0) ? 1 : 0;
            if(crc >= 0x80)
                crc = (uint8_t)(((crc << 1) + bit) ^ poly);
            else
                crc = (uint8_t)((crc << 1) + bit);
        }
    }
    thisFrame.buf[7] = crc;
    return crc;
}

uint32_t LeafMotorController::getTickInterval()
{
    return CFG_TICK_INTERVAL_MOTOR_CONTROLLER_LEAF;
}

void LeafMotorController::loadConfiguration() {
    config = (LeafMotorControllerConfiguration *)getConfiguration();

    if (!config) {
        config = new LeafMotorControllerConfiguration();
        setConfiguration(config);
    }

    MotorController::loadConfiguration(); // call parent

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
}

void LeafMotorController::saveConfiguration() {
    config = (LeafMotorControllerConfiguration *)getConfiguration();

    if (!config) {
        config = new LeafMotorControllerConfiguration();
        setConfiguration(config);
    }

    prefsHandler->write("CanbusNum", config->canbusNum);

    MotorController::saveConfiguration();
}

DMAMEM LeafMotorController leafMC;
