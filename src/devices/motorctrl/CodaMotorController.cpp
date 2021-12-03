/*
 * CodaMotorController.cpp
 *
 * CAN Interface to the Coda flavored UQM Powerphase 100 inverter -
   Handles sending of commands and reception of status frames to drive
   the inverter and thus motor.  Endianess is configurable in the firmware
   inside the UQM inverter but default is little endian.  This object module * uses little endian format
   - the least significant byte is the first in order with the MSB following.
 **************NOTE***************
  Ticks are critical for the UQM inverter.  A tick value of 10000 in config.h is necessary as the inverter
 expects a torque command within each 12 millisecond period.  Failing to provide it is a bit subtle to catch
  but quite dramatic.  The motor will run at speed for about 5 to 7 minutes and then "cough" losing all torque and
  then recovering.  Five minutes later, this will repeat.  Setting to a very fast value of 10000 seems to cure it NOW.
 As the software grows and the load on the CPU increases, this could show up again.
 *
Copyright (c) 2014 Jack Rickard

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

#include "CodaMotorController.h"

template<class T> inline Print &operator <<(Print &obj, T arg) {
    obj.print(arg);
    return obj;
}

long mss;
extern bool runThrottle;
const uint8_t swizzleTable[] = { 0xAA, 0x7F, 0xFE, 0x29, 0x52, 0xA4, 0x9D, 0xEF, 0xB, 0x16, 0x2C, 0x58, 0xB0, 0x60, 0xC0, 1 };


CodaMotorController::CodaMotorController() : MotorController()
{
    operationState = ENABLE;
    online = 0;
    activityCount = 0;
    sequence=0;
    commonName = "Coda UQM Powerphase 100 Inverter";
    shortName = "CodaUQM";
}

void CodaMotorController::earlyInit()
{
    prefsHandler = new PrefHandler(CODAUQM);
}

void CodaMotorController::setup()
{
    tickHandler.detach(this);

    Logger::info("add device: CODA UQM (id:%X, %X)", CODAUQM, this);

    loadConfiguration();

    MotorController::setup(); // run the parent class version of this function

    // register ourselves as observer of all 0x20x can frames for UQM
    canHandlerEv.attach(this, 0x200, 0x7f0, false);

    operationState=ENABLE;
    selectedGear=DRIVE;
    tickHandler.attach(this, CFG_TICK_INTERVAL_MOTOR_CONTROLLER_CODAUQM);
}


void CodaMotorController::handleCanFrame(const CAN_message_t &frame)
{
    int RotorTemp, invTemp, StatorTemp;
    int temp;
    online = 1; //if a frame got to here then it passed the filter and must come from UQM
    if (!running) //if we're newly running then cancel faults if necessary.
    {
        faultHandler.cancelOngoingFault(CODAUQM, FAULT_MOTORCTRL_COMM);
    }
    running=true;
    Logger::debug("UQM inverter msg: %X   %X   %X   %X   %X   %X   %X   %X  %X", frame.id, frame.buf[0],
                  frame.buf[1],frame.buf[2],frame.buf[3],frame.buf[4],
                  frame.buf[5],frame.buf[6],frame.buf[7]);

    switch (frame.id)
    {

    case 0x209:  //Accurate Feedback Message

        torqueActual =  ((((frame.buf[1] * 256) + frame.buf[0])-32128)) / 10.0f;
        dcVoltage = (((frame.buf[3] * 256) + frame.buf[2])-32128) / 10.0f;
        
        if(dcVoltage< 100.0f)
        {
            dcVoltage=100.0f;   //Lowest value we can display on dashboard
        }

        dcCurrent = (((frame.buf[5] * 256) + frame.buf[4])-32128) / 10.0f;
        speedActual = abs((((frame.buf[7] * 256) + frame.buf[6])-32128) / 2.0f);

        Logger::debug("UQM Actual Torque: %f DC Voltage: %f Amps: %f RPM: %u", torqueActual, dcVoltage, dcCurrent, speedActual);
        break;

    case 0x20A:    //System Status Message
        Logger::debug("UQM inverter 20A System Status Message Received");
        break;

    case 0x20B:    //Emergency Fuel Cutback Message
        Logger::debug("UQM inverter 20B Emergency Fuel Cutback Message Received");
        break;

    case 0x20C:    //Reserved Message
        Logger::debug("UQM inverter 20C Reserved Message Received");
        break;

    case 0x20D:    //Limited Torque Percentage Message
        Logger::debug("UQM inverter 20D Limited Torque Percentage Message Received");
        break;

    case 0x20E:     //Temperature Feedback Message

        invTemp = frame.buf[2];
        RotorTemp = frame.buf[3];
        StatorTemp = frame.buf[4];
        temperatureInverter = (invTemp-40);
        if (RotorTemp > StatorTemp) {
            temperatureMotor = (RotorTemp-40);
        }
        else {
            temperatureMotor = (StatorTemp-40);
        }
        Logger::debug("UQM 20E Inverter temp: %i Motor temp: %i", temperatureInverter,temperatureMotor);
        break;

    case 0x20F:    //CAN Watchdog Status Message
        Logger::debug("UQM 20F CAN Watchdog status error");
        warning=true;
        running=false;
        sendCmd2(); //If we get a Watchdog status, we need to respond with Watchdog reset
        break;
    }
}



void CodaMotorController::handleTick() {

    MotorController::handleTick(); //kick the ball up to papa
    sendCmd1();   //Send our lone torque command

    if(!online)  //This routine checks to see if we have received any frames from the inverter.  If so, ONLINE would be true and
    {   //we set the RUNNING light on.  If no frames are received for 2 seconds, we set running OFF.
        if (millis()-mss>2000)
        {
            running=false; // We haven't received any frames for over 2 seconds.  Otherwise online would be true.
            mss=millis();   //Reset our 2 second timer
        }
    }
    else running=true;
    online=false;//This flag will be set to true by received frames

}


/*
UQM only HAS a single command CAN bus frame - address 0x204  Everything is stuffed into this one frame. It has a 5 byte payload.

Byte 1 - always set to 0

Byte 2 - Command Byte
	Left four bits contain enable disable and forward reverse
		Bits 7/6 DISABLED =01
		Bits 7/6 ENABLE =10
		Bits 5/4 REVERSE=01
		Bits 5/4 FORWARD=10
		Example:  Enabled and Forward: 1010

	Right four bits (3 to 0) is a sequence counter that counts 0000 to 0111 and back to 0000

Byte 3 LSB of Torque command value.

Byte 4 MSB of Torque command value   Offset is 32128

Byte 5 Security CRC byte.

Bytes must be sent IN SEQUENCE and the CRC byte must be appropriate for bytes 2, 3, and 4.
Values above 32128 are positive torque.  Values below 32128 are negative torque
*/

void CodaMotorController::sendCmd1()
{
    CodaMotorControllerConfiguration *config = (CodaMotorControllerConfiguration *)getConfiguration();

    CAN_message_t output;
    output.len = 5;
    output.id = 0x204;
    output.flags.extended = 0; //standard frame
    output.buf[0] = 0x00; //First byte is always zero.


    if(operationState==ENABLE)
    {
        output.buf[1] = 0x80; //1000 0000
    }
    else
    {
        output.buf[1] = 0x40; //0100 0000
    }

    if(selectedGear==DRIVE)
    {
        output.buf[1] |= 0x20; //xx10 0000
    }
    else
    {
        output.buf[1] |= 0x10;//xx01 0000
    }

    sequence+=1; //Increment sequence
    if (sequence==8) {
        sequence=0;   //If we reach 8, go to zero
    }
    output.buf[1] |= sequence; //This should retain left four and add sequence count
    //to right four bits.
    //Requested throttle is [-1000, 1000]
    //Two byte torque request in 0.1NM Can be positive or negative

    torqueRequested = ((throttleRequested * config->torqueMax) / 100.0f); //Calculate torque request from throttle position x maximum torque
 
    //If our requested torque is a negative number, we are in regen.  Let's use taper values to taper it below threshold rpm
    if(torqueRequested < 0 && speedActual < config->regenTaperUpper)  //We are in regen and below regenTaperUpper value
      {    
        
        if (speedActual < config->regenTaperLower) torqueRequested = 0; //If less than lower taper, NO regen
        
        else {        
              int32_t range = config->regenTaperUpper - config->regenTaperLower; 
              int32_t taper = speedActual - config->regenTaperLower;
              int32_t calc = (torqueRequested * taper) / range;
              torqueRequested = (int16_t)calc;  //A tapered REGEN value function of RPM within range upper/lower.
            }
      }

    //If overspeed, let's cut torque in half.  If not, add requested torque to torqueCommand
      
    if(speedActual>config->speedMax) {torqueRequested /= 2.0f;}   //If actual rpm greater than max rpm, add torque command to offset
    
     torqueCommand = 32128 + torqueRequested; //Torque command 32128 is zero torque.  Values below are regen.  Above are torque.
  
    
   /* if (speedActual < 1700 && torqueCommand < 32128) {
        Logger::debug(CODAUQM, "Canceling regen at low speed");
        int32_t working = torqueCommand - 32128; //remove bias;
        int comp = speedActual - 200;
        if (comp < 0) comp = 0;
        working *= comp;
        working /= 1500;
        torqueCommand = 32128 + working; //limited regen request
    }*/
    
    output.buf[3] = (torqueCommand & 0xFF00) >> 8;  //Stow torque command in bytes 2 and 3.
    output.buf[2] = (torqueCommand & 0x00FF);
    output.buf[4] = genCodaCRC(output.buf[1], output.buf[2], output.buf[3]); //Calculate security byte

    canHandlerEv.sendFrame(output);  //Mail it.
    timestamp();

    Logger::debug("Torque command: %X   %X  ControlByte: %X  LSB %X  MSB: %X  CRC: %X  %d:%d:%d.%d",output.id, output.buf[0],
                  output.buf[1],output.buf[2],output.buf[3],output.buf[4], hours, minutes, seconds, milliseconds);

}


void CodaMotorController::sendCmd2() {
    CodaMotorControllerConfiguration *config = (CodaMotorControllerConfiguration *)getConfiguration();

    /*In the CODA CAN bus capture logs, this command, defined in the UQM manual as a
      207 watchdog reset, is sent every 480msec.  It also always occurs after the last count of
      a sequence ie 57, 67, 97, or A7.  But this may just be coincidence.
      By the book, this is to be sent in response to a watchdog timeout.
      We send this in response to receipt of a 20F Watchdog status.
      */

    CAN_message_t output;
    output.len = 8;
    output.id = 0x207;
    output.flags.extended = 0; //standard frame
    output.buf[0] = 0xa5; //This is simply three given values.  The 5A appears to be
    output.buf[1] = 0xa5; //the important one.
    output.buf[2] = 0x5a;
    output.buf[3] = 0x00;
    output.buf[4] = 0x00;
    output.buf[5] = 0x00;
    output.buf[6] = 0x00;
    output.buf[7] = 0x00;

    canHandlerEv.sendFrame(output);
    timestamp();
    Logger::debug("Watchdog reset: %X  %X  %X  %d:%d:%d.%d",output.buf[0], output.buf[1],
                  output.buf[2], hours, minutes, seconds, milliseconds);

    warning=false;
}


DeviceId CodaMotorController::getId() {
    return (CODAUQM);
}

uint32_t CodaMotorController::getTickInterval()
{
    return CFG_TICK_INTERVAL_MOTOR_CONTROLLER_CODAUQM;
}

void CodaMotorController::loadConfiguration() {
    CodaMotorControllerConfiguration *config = (CodaMotorControllerConfiguration *)getConfiguration();

    if (!config) {
        config = new CodaMotorControllerConfiguration();
        setConfiguration(config);
    }

    MotorController::loadConfiguration(); // call parent

}

void CodaMotorController::saveConfiguration() {
    MotorController::saveConfiguration();
}

uint8_t CodaMotorController::genCodaCRC(uint8_t cmd, uint8_t torq_lsb, uint8_t torq_msb)
{
    int counter;
    uint8_t crc;
    uint16_t temp_torq = torq_lsb + (256 * torq_msb);
    crc = 0x7F; //7F is the answer if bytes 3 and 4 are zero. We build up from there.

    //this can be done a little more efficiently but this is clearer to read
    if (((cmd & 0xA0) == 0xA0) || ((cmd & 0x60) == 0x60)) temp_torq += 1;

    //Not sure why this happens except to obfuscate the result
    if ((temp_torq % 4) == 3) temp_torq += 4;

    //increment over the bits within the torque command
    //and applies a particular XOR for each set bit.
    for (counter = 0; counter < 16; counter++)
    {
        if ((temp_torq & (1 << counter)) == (1 << counter)) crc = (byte)(crc ^ swizzleTable[counter]);
    }
    return (crc);
}



void CodaMotorController::timestamp()
{
    milliseconds = (int) (millis()/1) %1000 ;
    seconds = (int) (millis() / 1000) % 60 ;
    minutes = (int) ((millis() / (1000*60)) % 60);
    hours   = (int) ((millis() / (1000*60*60)) % 24);
    // char buffer[9];
    //sprintf(buffer,"%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
    // Serial<<buffer<<"\n";
}

CodaMotorController codaMC;

