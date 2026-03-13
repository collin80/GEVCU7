/*
 * SevconMotorController.cpp
 
Driver for Sevcon Gen5 motor controller.
Uses J1939 for communications
 
Copyright (c) 2026 Collin Kidder

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

#include "SevconMotorController.h"

template<class T> inline Print &operator <<(Print &obj, T arg) {
    obj.print(arg);
    return obj;
}

SevconMotorController::SevconMotorController() : MotorController()
{
    operationState = ENABLE;
    sequence = 0;
    faultCode = 0;
    isLockedOut = false;
    inSDOUpload = false;
    commonName = "Sevcon Gen 5 Inverter";
    shortName = "SevconInv";
	deviceId = SEVCONINV;
}

void SevconMotorController::setup()
{
    tickHandler.detach(this);

    Logger::info("add device: Sevcon Inverter (id:%X, %X)", SEVCONINV, this);

    loadConfiguration();

    MotorController::setup(); // run the parent class version of this function

    ConfigEntry entry;
    //        cfgName          helpText                              variable ref        Type                   Min Max Precision Funct
    entry = {"SEVCON-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);

    setAttachedCANBus(config->canbusNum);

    //all J1939 messages come in with the format 0x18??FF71 so we need only look for the unique
    //values in that one byte to decode the traffic
    attachedCANBus->attach(this, 0x01900FF71, 0x1F00FFFF, true);
    setNodeID(1); //we're interested in traffic from node 1 and to node 1
    setCANOpenMode(true); //enable CANOpen processing so we can send SDOs to the inverter

    //these are sent too along with other CanOpen traffic but not sure we need to care
    //116 0-1 = Velocity 2-3 = Heatsink temp rest of the bytes are Id but we don't care 
    //481 0-1 = Motor temp, 2-3 = battery current 4-5 = torque demand 6-7 torque actual
    
    //J1939 PGNs for motor control. Send these to inverter:

    //(ProCAN Motor Demands Gen 5) Doesn't seem to actually be the way to control the Gen5. 
    //Our addr 0xFA, inverter address 0x71 send 200ms interval or faster?
    //18300 Vbatt high limit, vbatt low limit
    //18200 iBatt discharge limit, iBatt regen limit
    //18100 (speed command) command, fwd speed limit, reverse speed limit
    //18000 (Trq) Torque demand, trq drive limit, trq regen limit
    //All above have byte 6 = sequence 7 = checksum
    //Not so sure this ProCAN stuff is relevant. Try not using it at all.
    
    //(H protocol) HC (Motor Demands HVLP / D8 / Gen5)
    //HC1 11000 (Trq Demand, Control Word, Trq drive limit)
    //HC2 11100 (Trq Regen Limit, Fwd Speed limit, Rev Speed limit)
    //HC3 11200 (Batt discharge limit, Batt regen limit, V cap target)

    //G protocol (Generator control) - Does not seem necessary
    //GC1 0FF64 (E Stop, Enable, V Target, I Max Discharge, I max regen)

    //Inverter sends j1939 messages:
    //HS1 11800 = Torque feedback (Trq Measured, Motor Speed, iBatt)
    //HS2 11900 = Inverter status feedback (Trq Max Forward, Trq Max Reverse, status word, limit code) torques 2 bytes then status then limit
    //HS3 11A00 = Temperature feedback (T Controller, T Motor, VCap) temps 2 bytes, seem to be in full degrees and signed
    //HS4 11B00 = Fault info (Fault Code 2 bytes, Fault Data next 4?) 
    //HS5 11C00 = Encoder info (Encoder position) (first 4 bytes?) Very fine scale
    //HD1 12800 = Debug currents (Id, Iq, IdRef, IqRef) (seems to be in 1/16 of an amp per value)
    //HD2 12900 = Debug voltages (Ud, Uq, ModIdx, Trq Control Target)
    //HD3 12A00 = Debug temperatures A (Temp Junction 0, Temp Junction 1, TJ2, TJ3, TJ4, TJ5, TMotor Measured, TMotor Estimate)
    //HD4 12B00 = Debug temperatures B (T Cap Fast, T Track, T Cap Slow, T Heatsink, TJ Delta, T Device, T Motor 2, T Motor 3)
    //HD5 12C00 = Debug control PMAC (Max Flux, LV Lim, Iq Max, Ls)
    //debug temperatures seem to be in decimal when viewed as hex? That's weird!
    //all multibyte values seem to be little endian

	setAlive();

    attachedCANBus->setMasterID(0x7a);

    operationState = ENABLE;
    tickHandler.attach(this, CFG_TICK_INTERVAL_MOTOR_CONTROLLER);
}


void SevconMotorController::handleCanFrame(const CAN_message_t &frame)
{
    uint8_t *data = (uint8_t *)frame.buf;
    setAlive();
	
    if (!running) //if we're newly running then cancel faults if necessary.
    {
        faultHandler.cancelOngoingFault(SEVCONINV, COMM_TIMEOUT);
    }
    
    running = true;
    
    Logger::debug("inverter msg: %X   %X   %X   %X   %X   %X   %X   %X  %X", frame.id, frame.buf[0],
                  frame.buf[1],frame.buf[2],frame.buf[3],frame.buf[4],
                  frame.buf[5],frame.buf[6],frame.buf[7]);

    //inverter sends values as low byte followed by high byte.
    //need only look at third byte of ID
    uint8_t msgType = (frame.id & 0xFF0000) >> 16;
    uint16_t fault;
    uint8_t statusFlags;
    uint8_t limitCode;
    SDO_FRAME reqFrame;
    switch (msgType)
    {
    case 0x18: //HS1 Torque feedback (Trq Measured, Motor Speed, iBatt)
        //so far as I can tell, speed and battery amps seem to be in full RPM and A respectively
        //Torque is in Nm but likely scaled by some amount.
        this->torqueActual = (data[0] | (data[1] << 8)) * 0.0625; //torque in bytes 0 and 1, 1/16th scaling?
        this->speedActual = data[2] | (data[3] << 8); //speed in bytes 2 and 3, in RPM
        this->dcCurrent = (data[4] | (data[5] << 8)); //current in bytes 4 and 5
        break;
    case 0x19: //HS2 Inverter status feedback (Trq Max Forward, Trq Max Reverse, status word, limit code) torques 2 bytes then status then limit 
        //once again, torque will be in Nm but scaled. Not sure of what status word and limit code actually look like yet
        //bytes 0 and 1 are torque max forward limit
        //bytes 2 and 3 are torque max reverse limit
        statusFlags = data[4];
        limitCode = data[5];
        Logger::debug("Inverter status flags: %X   Limit code: %X", statusFlags, limitCode);
        break;
    case 0x1A: //HS3 Temperature feedback (T Controller, T Motor, VCap) temps 2 bytes, seem to be in full degrees and signed 
        //temps are full degrees C and signed. Vcap is in volts but scaled by some amount. Might be 1/4 scaling
        this->temperatureInverter = (int16_t)(data[0] | (data[1] << 8)); //inverter temp in bytes 0 and 1, signed int
        this->temperatureMotor = (int16_t)(data[2] | (data[3] << 8)); //motor temp in bytes 2 and 3, signed int
        this->dcVoltage = (data[4] | (data[5] << 8)) * 0.25; //Vcap in bytes 4 and 5, 1/4 scaling?
        break;
    case 0x1B: //HS4 Fault info (Fault Code 2 bytes, Fault Data next 4?) 
        //no idea what the fault codes mean
        //but, CANOpen could be used to get fault descriptions.
        fault = data[0] | (data[1] << 8);
        if (faultCode != fault)
        {
            faultCode = fault;
            Logger::error("Inverter fault code: %X", faultCode);
            //send out an SDO request to get the fault description.
            reqFrame.targetID = 01; //node ID of the inverter for CANOpen
            reqFrame.cmd.cmdStruct.cmdType = SDO_WRITE;
            //to get fault desc, write to sub idx 2 the value 0 to get most serious fault ID.
            //Then read from subidx 3 to get that fault ID.
            //With that fault ID, write it to 0x5610 sub idx 1
            //Then read from sub idx 2 to get the description of that fault.
            reqFrame.index = 0x5300;
            reqFrame.subIndex = 2;
            reqFrame.dataLength = 1;
            reqFrame.data[0] = 0; //we want the most severe fault code
            attachedCANBus->sendSDORequest(reqFrame);
            reqFrame.subIndex = 3;
            reqFrame.cmd.cmdStruct.cmdType = SDO_READ;
            reqFrame.dataLength = 0;
            attachedCANBus->sendSDORequest(reqFrame); //read that fault ID
        }
        break;
    case 0x1C: //HS5 Encoder info (Encoder position) (first 4 bytes?) Very fine scale 
        //no big reason to decode this value or care.
        break;
	}
 }

void SevconMotorController::handleSDORequest(SDO_FRAME &frame)
{
    //inverter should not be sending requests to us. I don't see this being used
}

void SevconMotorController::handleSDOResponse(SDO_FRAME &frame)
{
    if (frame.cmd.cmdStruct.cmdType == SDO_WRITEACK)
    {
        if (frame.index == 0x5300)
        {
            if (frame.subIndex == 2)
            {
                uint16_t faultID = frame.data[0] | (frame.data[1] << 8);
                Logger::debug("Inverter fault ID: %X", faultID);
                SDO_FRAME reqFrame;
                reqFrame.targetID = 01; //node ID of the inverter for CANOpen
                reqFrame.cmd.cmdStruct.cmdType = SDO_WRITE;
                reqFrame.index = 0x5610;
                reqFrame.subIndex = 1;
                reqFrame.dataLength = 2;
                reqFrame.data[0] = (faultID & 0x00FF);
                reqFrame.data[1] = (faultID & 0xFF00) >> 8;
                attachedCANBus->sendSDORequest(reqFrame); //write that fault ID to the location to get the description
                reqFrame.cmd.cmdStruct.cmdType = SDO_READ;
                reqFrame.index = 0x5610;
                reqFrame.subIndex = 2;
                reqFrame.dataLength = 0;
                attachedCANBus->sendSDORequest(reqFrame); //read the description of that fault.
            }
        }
    }

    if (frame.index == 0x5300 && frame.subIndex == 2)
    {
        frame.data[frame.dataLength] = 0; //null terminate just in case
        Logger::error("Inverter fault description: %s", frame.data);
    }
}

void SevconMotorController::handleTick() {

    MotorController::handleTick(); //kick the ball up to papa
	
	//Send out control message if inverter tells us it's set to CAN control. Otherwise just listen
    if (isCANControlled)
	{
		checkAlive(1000);
		sendCmdFrame();
	}

    if(isOperational)  //This routine checks to see if we have received any frames from the inverter.  If so, ONLINE would be true and
    {
		running = true;
    }
    else running = false;
}


void SevconMotorController::sendCmdFrame()
{
	Gears currentGear = getSelectedGear();

    static uint8_t counter;

    counter++;

    CAN_message_t output;
    output.len = 8;
    output.id = 0x191071FA; //target 0x71, we're 0xFA, this is PGN 11000 HC1
    output.flags.extended = 1; //extended id frame
	
	//the comment was hallucinated by the AI. But, it's as good a guess as any. 
    output.buf[2] = 0; //Control word. Bit 0 = enable, bit 1 = direction (0=fwd, 1=rev)
    if(operationState == ENABLE && currentGear != NEUTRAL)
    {
        output.buf[2] = 1;
    }
    else
    {
        output.buf[2] = 0;
    }

    if(currentGear == DRIVE)
    {
        output.buf[2] |= 2; //forward
    }
    else
    {
        //output.buf[2] |= 4; //reverse
    }
    
    
    torqueRequested = ((throttleRequested * config->torqueMax) / 100.0f); //Calculate torque request from throttle position x maximum torque
    if(speedActual<config->speedMax) {
        torqueCommand = torqueRequested;   //If actual rpm less than max rpm, add torque command to offset
    }
    else {
        torqueCommand = torqueRequested / 2.0f;   //If at RPM limit, cut torque command in half.
    }
    
    if (torqueRequested < 0) torqueRequested = 0;
    
    Logger::debug("ThrottleRequested: %i     TorqueRequested: %i", throttleRequested, torqueRequested);
	
    output.buf[1] = (torqueCommand & 0xFF00) >> 8;  //Stow torque command in bytes 0 and 1.
    output.buf[0] = (torqueCommand & 0x00FF);

    //torque drive limit but I have no idea of scaling. 10FF is 4351 whic is a decently large number
    //as a complete guess. 
    output.buf[4] = 0xFF;
    output.buf[5] = 0x10;
    output.buf[6] = counter; //sequence number in byte 6 

    uint8_t checksum = 0;
    for (int i = 0; i < 7; i++)
    {
        checksum += output.buf[i];
    }
    output.buf[7] = checksum;
    
    attachedCANBus->sendFrame(output);  //Mail it.

//    Logger::debug("CAN Command Frame: %X  %X  %X  %X  %X  %X  %X  %X",output.id, output.buf[0],
//                  output.buf[1],output.buf[2],output.buf[3],output.buf[4],
//				  output.buf[5],output.buf[6],output.buf[7]);

    output.id = 0x191171FA; //target 0x71, we're 0xFA, this is PGN 11100 HC2
    output.buf[0] = 0xFF;
    output.buf[1] = 0x03; //much lower regen torque limit
    output.buf[2] = (config->speedMax & 0xFF); //forward speed limit. Seemingly just in 1 RPM increments
    output.buf[3] = (config->speedMax & 0xFF00) >> 8;
    output.buf[4] = output.buf[2];
    output.buf[5] = output.buf[3] / 2; //half the forward speed, just for testing
    output.buf[6] = counter; //sequence number in byte 6

    checksum = 0;
    for (int i = 0; i < 7; i++)
    {
        checksum += output.buf[i];
    }
    output.buf[7] = checksum;
    attachedCANBus->sendFrame(output);  //Mail it.

    output.id = 0x191271FA; //target 0x71, we're 0xFA, this is PGN 11200 HC3
    output.buf[0] = 0x90; //bat discharge limit. No proof of scale. Guessing 1/4A scaling
    output.buf[1] = 0x01; //100A if scale really is 0.25 
    output.buf[2] = 0x70;
    output.buf[3] = 0x01; //Slightly less
    output.buf[4] = 0x95; //slightly more. THis is VCAP target but no idea what that is.
    output.buf[5] = 0x01; 
    output.buf[6] = counter; //sequence number in byte 6

    checksum = 0;
    for (int i = 0; i < 7; i++)
    {
        checksum += output.buf[i];
    }
    output.buf[7] = checksum;
    attachedCANBus->sendFrame(output);  //Mail it.

    attachedCANBus->sendHeartbeat();
    
}

uint32_t SevconMotorController::getTickInterval()
{
    return CFG_TICK_INTERVAL_MOTOR_CONTROLLER;
}

void SevconMotorController::loadConfiguration()
{
    config = (SevconMotorControllerConfiguration *)getConfiguration();

    if (!config) {
        config = new SevconMotorControllerConfiguration();
        setConfiguration(config);
    }

    MotorController::loadConfiguration(); // call parent

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
}

void SevconMotorController::saveConfiguration()
{
    config = (SevconMotorControllerConfiguration *)getConfiguration();
    
    prefsHandler->write("CanbusNum", config->canbusNum);

    MotorController::saveConfiguration();
}

const char* SevconMotorController::getFaultDescription(uint16_t faultcode)
{
    return nullptr; //no match, return nothing
}

DMAMEM SevconMotorController sevconMC;