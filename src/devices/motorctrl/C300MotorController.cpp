/*
 * C300MotorController.cpp
 *
 *
Copyright (c) 2021 EVTV

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

#include "C300MotorController.h"

C300MotorController::C300MotorController() : MotorController() {    
    selectedGear = NEUTRAL;
    operationState = DISABLED;
    actualState = DISABLED;
    online = 0;
    activityCount = 0;
//	maxTorque = 2000;
    commonName = "C300 Inverter";
    shortName = "C300Inv";
}

void C300MotorController::earlyInit()
{
    prefsHandler = new PrefHandler(C300INV);
}

void C300MotorController::setup() {
    tickHandler.detach(this);

    Logger::info("add device: C300 (id:%X, %X)", C300INV, this);

    loadConfiguration();
    MotorController::setup(); // run the parent class version of this function

    //CFF7902, CFF7B02, CFF7A02, CFF7C02 are the addresses. Only the lower nibble of the second byte changes
    //So, set up to filter and let anything in that nibble through but otherwise match exactly
    canHandlerBus0.attach(this, 0x0CFF7802, 0x0FFFF0FF, true);

    running = false;
    setPowerMode(modeTorque);
    setSelectedGear(NEUTRAL);
    setOpState(DISABLED );
    ms=millis();

    tickHandler.attach(this, CFG_TICK_INTERVAL_MOTOR_CONTROLLER_C300);
}

void C300MotorController::handleCanFrame(const CAN_message_t &frame) {
    int RotorTemp, invTemp, StatorTemp;
    int temp;
    online = true; //if a frame got to here then it passed the filter and must have been from the DMOC

    Logger::debug("C300 CAN received: %X  %X  %X  %X  %X  %X  %X  %X  %X", frame.id,frame.buf[0] ,frame.buf[1],frame.buf[2],frame.buf[3],frame.buf[4],frame.buf[5],frame.buf[6],frame.buf[7]);

    switch (frame.id) 
    {
    case 0x0CFF7902: //MCU status, mode, output torque, etc
        //byte 0 first three bits are error status. 0 = good, higher numbers are progressively worse
        //Upper nibble of byte 0 is counter
        //Byte 1 bits 0-1 are mode. 0 = standby, 1 = Speed ctrl, 2 = torque ctrl, 3 = discharge
        //Byte 1 bit 2 is enable status (1 = enabled)
        //Byte 1 bit 3 = precharge allowed (1 = yes, 0 = no)
        //byte 1 bit 4 = precharge complete (0 = unfinished, 1 = done!)
        //bytes 2-3 = motor torque (0.25nm scale 5000 offset just like command)
        //bytes 4-5 = motor speed (12000 offset, 1 RPM scale just like command)
        faulted = ((frame.buf[0] & 7) > 0)?true:false;
        ready = ((frame.buf[1] & 3) == 2)?true:false;
        prechargeComplete = ((frame.buf[1] & 8) == 8) ? true : false;
        speedActual = ((frame.buf[4] * 256) + frame.buf[5]) - 12000;
        torqueActual = (((frame.buf[2] * 256) + frame.buf[3]) / 4.0f) - 5000;
        activityCount++;
        break;
    case 0x0CFF7B02: //torque limits and temperatures
        //byte 0 upper nibble = counter
        //bytes 1-2 = Torque limit in generate mode (1 scale, 5000 offset)
        //bytes 3-4 = Torque limit in motoring mode (1 scale, no offset)
        //byte 5 = IGBT temperature (-40 offset)
        //byte 6 = Motor temperature (-40 offset)
        //byte 7 = Motor controller temperature (-40 offset)
        torqueAvailable = ((frame.buf[3] * 256) + frame.buf[4]);
        temperatureMotor = frame.buf[6] - 40;
        temperatureInverter = frame.buf[5] - 40;
        temperatureSystem = frame.buf[7] - 40;
        activityCount++;
        break;
    case 0x0CFF7A02: //input voltage and current
        //byte 0 upper nibble = counter
        //byte 1-2 = controller input DC voltage (1 scale, no offset)
        //byte 3-4 = controller input DC amperage (1 scale 1000 offset)
        dcVoltage = ((frame.buf[1] * 256) + frame.buf[2]);
        dcCurrent = (((frame.buf[3] * 256) + frame.buf[4])) - 1000;
        activityCount++;
        break;
    case 0x0CFF7C02: //fault reporting
        //byte 0 bits 0-1 = motor overspeed fault (0 = no, 3 = its bad!)
        //byte 0 bits 4-5 = motor over temp (0 = no, progressively worse 1,2,3)
        //byte 1 bits 4-5 = HW fault (0 = no, 1= warning, 3 = BAD)
        //byte 2 bits 4-5 = IGBT fault (0 = no, 3 = DOOM)
        //byte 2 bits 6-7 = IGBT overtemp (0 = no, 3 = crispy!)
        //byte 3 bits 0-1 = Over current (0 = no, 3 = slag)
        //byte 3 bits 2-3 = DC over voltage (same as all the above)
        //byte 3 bits 4-5 = DC under voltage
        //Byte 3 bits 6-7 = IGBT temp sensor fault?
        //byte 4 bits 0-1 = CAN Fault (HTF would we know that then?!)
        //Byte 4 bits 2-3 = motor temperature sensor fault
        //Byte 5 bits 4-5 = motor tuning fault (position faulty?)
        activityCount++;
        break;
    }
}

void C300MotorController::handleTick() {

    MotorController::handleTick(); //kick the ball up to papa

    if (activityCount > 0)
    {
        activityCount--;
        if (activityCount > 60) activityCount = 60;
        if (activityCount > 40) //If we are receiving regular CAN messages from DMOC, this will very quickly get to over 40. We'll limit
            // it to 60 so if we lose communications, within 20 ticks we will decrement below this value.
        {
            Logger::debug("Enable Input Active? %u         Reverse Input Active? %u" ,systemIO.getDigitalIn(getEnableIn()),systemIO.getDigitalIn(getReverseIn()));
            if(getEnableIn()<0)setOpState(ENABLE); //If we HAVE an enableinput 0-3, we'll let that handle opstate. Otherwise set it to ENABLE
            if(getReverseIn()<0)setSelectedGear(DRIVE); //If we HAVE a reverse input, we'll let that determine forward/reverse.  Otherwise set it to DRIVE
        }
    }
    else {
        setSelectedGear(NEUTRAL); //We will stay in NEUTRAL until we get at least 40 frames ahead indicating continous communications.
    }

    if(!online)  //This routine checks to see if we have received any frames from the inverter.  If so, ONLINE would be true and
    {   //we set the RUNNING light on.  If no frames are received for 2 seconds, we set running OFF.
        if ((millis()-ms)>2000)
        {
            running=false; // We haven't received any frames for over 2 seconds.  Otherwise online would be true.
            ms=millis();   //Reset our 2 second timer
        }
    }
    else running=true;
    online=false;//This flag will be set to 1 by received frames.


    sendCmd();  //This actually sets our GEAR and our actualstate cycle
}

//This inverter is controlled by only one ID. We send all commands in here
//byte 0, bit 0 = MCU Enable
//byte 0, bit 1 = Direction (0 = forward, 1 = reverse)
//byte 0, bits 2-3 = Operation command. 0 = standby, 1 = speed mode, 2 = torque mode, 3 = discharge. Only use torque mode
//byte 0, bits 4-7 = heartbeat. Inc constantly, allow wrap
//byte 1, bit 0 = Brake Valid. 0 = No brake, 1 = Brake. Don't know what this does.
//byte 1, bit 1-7 then bytes 2,3 are all reserved. Send 0's
//Byte 4-5 is torque request in 0.25NM with a -5000 offset
//Byte 6-7 is speed request in 1RPM with -12000 offset but speed mode is not likely supported so probably just send a value of 12000 here
void C300MotorController::sendCmd() 
{
    C300MotorControllerConfiguration *config = (C300MotorControllerConfiguration *)getConfiguration();
    CAN_message_t output;
    OperationState newstate;
    alive = (alive + 1) & 0x0F;
    output.len = 8;
    output.id = 0x0CFF1401;
    output.flags.extended = 1; //29 bit ID, extended frame

    torqueCommand = 20000; //set offset  for zero torque commanded (5000 / 0.25)

    Logger::debug("Throttle requested: %i", throttleRequested);

    torqueRequested = 0;
    if (actualState == ENABLE) { //don't even try sending torque commands until the DMOC reports it is ready
        //if (selectedGear == DRIVE) {
            //torqueMax is in Nm, throttle is -1000 to +1000 but we want the output to be
            //in 1/4 of a Nm. Divide by 1000 to get scale then multiply by 4 so really divide by 250
            torqueRequested = (((long) throttleRequested * (long) config->torqueMax) / 250);
            //if (speedActual < config->regenTaperUpper && torqueRequested < 0) taperRegen();
        //}
        //if (selectedGear == REVERSE) {
        //    torqueRequested = (((long) throttleRequested * -1 *(long) config->torqueMax) / 1000L);//If reversed, regen becomes positive torque and positive pedal becomes regen.  Let's reverse this by reversing the sign.  In this way, we'll have gradually diminishing positive torque (in reverse, regen) followed by gradually increasing regen (positive torque in reverse.)
            //if (speedActual < config->regenTaperUpper && torqueRequested > 0) taperRegen();
        // }
    }

    if(speedActual < config->speedMax) {
        torqueCommand+=torqueRequested;   //If actual rpm is less than max rpm, add torque to offset
    }
    else 
    {
        torqueCommand += (torqueRequested / 1.3f);   // else torque is reduced
    }
    
    speedRequested = 12000;
    if (selectedGear == NEUTRAL) setOpState(DISABLED);
                                                                                          // torque mode
    output.buf[0] = ((operationState == ENABLE)?1:0) | ((selectedGear == DRIVE)?0:2) | (2 << 2) | (alive << 4);
    output.buf[1] = 0; //0 at the low bit means brake valid. What brake?
    output.buf[2] = 0; //all reserved bits
    output.buf[3] = 0; //also all reserved bits
    output.buf[4] = (torqueCommand & 0xFF00) >> 8;
    output.buf[5] = (torqueCommand & 0x00FF);
    output.buf[6] = (speedRequested & 0xFF00) >> 8;
    output.buf[7] = (speedRequested & 0x00FF);
 
    Logger::debug("C300 Command tx: %X %X %X %X %X %X %X %X", output.buf[0], output.buf[1], output.buf[2], output.buf[3],
                  output.buf[4], output.buf[5], output.buf[6], output.buf[7]);

    canHandlerBus0.sendFrame(output);
}

//I don't believe motor controllers need to handle regen taper themselves. 
//The other classes in series (Throttle and base class) should handle that. Check into why this is here.
void C300MotorController::taperRegen()
{
    C300MotorControllerConfiguration *config = (C300MotorControllerConfiguration *)getConfiguration();
    if (speedActual < config->regenTaperLower) torqueRequested = 0;
    else {        
        int32_t range = config->regenTaperUpper - config->regenTaperLower; //next phase is to not hard code this
        int32_t taper = speedActual - config->regenTaperLower;
        int32_t calc = (torqueRequested * taper) / range;
        torqueRequested = (int16_t)calc;
    }
}

DeviceId C300MotorController::getId() {
    return (C300INV);
}

uint32_t C300MotorController::getTickInterval()
{
    return CFG_TICK_INTERVAL_MOTOR_CONTROLLER_C300;
}

void C300MotorController::loadConfiguration() {
    C300MotorControllerConfiguration *config = (C300MotorControllerConfiguration *)getConfiguration();

    if (!config) {
        config = new C300MotorControllerConfiguration();
        setConfiguration(config);
    }

    MotorController::loadConfiguration(); // call parent
}

void C300MotorController::saveConfiguration() {
    MotorController::saveConfiguration();
}

void C300MotorController::timestamp()
{
    milliseconds = (int) (millis()/1) %1000 ;
    seconds = (int) (millis() / 1000) % 60 ;
    minutes = (int) ((millis() / (1000*60)) % 60);
    hours   = (int) ((millis() / (1000*60*60)) % 24);
    // char buffer[9];
    //sprintf(buffer,"%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
    // Serial<<buffer<<"\n";
}

C300MotorController C300MC;