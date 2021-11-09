/*
 * RMSMotorController.cpp
 
   Driver to interface with Rinehart Motion PM series motor controllers. The inverter itself is very competent
   and could directly interface with a pedal itself. In that case we'd just be monitoring the status. Otherwise
   we will sent drive commands. Either option should be supported by this code.
 
Copyright (c) 2017 Collin Kidder

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

#include "RMSMotorController.h"

template<class T> inline Print &operator <<(Print &obj, T arg) {
    obj.print(arg);
    return obj;
}

RMSMotorController::RMSMotorController() : MotorController()
{
    operationState = ENABLE;
    online = 0;
    activityCount = 0;
    sequence = 0;
    isLockedOut = true;
    commonName = "Rinehart Motion Systems Inverter";
    shortName = "RMSInverter";
}

void RMSMotorController::earlyInit()
{
    prefsHandler = new PrefHandler(RINEHARTINV);
}

void RMSMotorController::setup()
{
    tickHandler.detach(this);

    Logger::info("add device: Rinehart Inverter (id:%X, %X)", RINEHARTINV, this);

    loadConfiguration();

    MotorController::setup(); // run the parent class version of this function

    //allow through 0xA0 through 0xAF	
    canHandlerEv.attach(this, 0x0A0, 0x7f0, false);

    operationState = ENABLE;
    selectedGear = NEUTRAL;
    tickHandler.attach(this, CFG_TICK_INTERVAL_MOTOR_CONTROLLER);
}


void RMSMotorController::handleCanFrame(const CAN_message_t &frame)
{
    int temp;
    uint8_t *data = (uint8_t *)frame.buf;
    online = 1; //if a frame got to here then it passed the filter and must come from RMS
	
    if (!running) //if we're newly running then cancel faults if necessary.
    {
        faultHandler.cancelOngoingFault(RINEHARTINV, FAULT_MOTORCTRL_COMM);
    }
    
    running = true;
    
    Logger::debug("inverter msg: %X   %X   %X   %X   %X   %X   %X   %X  %X", frame.id, frame.buf[0],
                  frame.buf[1],frame.buf[2],frame.buf[3],frame.buf[4],
                  frame.buf[5],frame.buf[6],frame.buf[7]);

    //inverter sends values as low byte followed by high byte.
    switch (frame.id)
    {
    case 0xA0: //Temperatures 1 (driver section temperatures)
	    handleCANMsgTemperature1(data);
        break;
    case 0xA1: //Temperatures 2 (ctrl board and RTD inputs)
        handleCANMsgTemperature2(data);
	    break;
    case 0xA2: //Temperatures 3 (More RTD, Motor Temp, Torque Shudder)
	    handleCANMsgTemperature3(data);
	    break;
    case 0xA3: //Analog input voltages
        handleCANMsgAnalogInputs(data);	    
	    break;
    case 0xA4: //Digital input status
        handleCANMsgDigitalInputs(data);
	    break;
    case 0xA5: //Motor position info
        handleCANMsgMotorPos(data);
        break;
    case 0xA6: //Current info
        handleCANMsgCurrent(data);	    
	    break;
    case 0xA7: //Voltage info
		handleCANMsgVoltage(data);
		break;
    case 0xA8: //Flux Info
        handleCANMsgFlux(data);
  	    break;
    case 0xA9: //Internal voltages
        handleCANMsgIntVolt(data);
		break;
    case 0xAA: //Internal states
        handleCANMsgIntState(data);
		break;
    case 0xAB: //Fault Codes
        handleCANMsgFaults(data);
		break;
    case 0xAC: //Torque and Timer info
        handleCANMsgTorqueTimer(data);
		break;
    case 0xAD: //Mod index and flux weakening info
        handleCANMsgModFluxWeaken(data);
		break;
    case 0xAE: //Firmware Info
        handleCANMsgFirmwareInfo(data);   	
	    break;
    case 0xAF: //Diagnostic Data
		handleCANMsgDiagnostic(data);			
	    break;
	}
}

void RMSMotorController::handleCANMsgTemperature1(uint8_t *data)
{
	int igbtTemp1, igbtTemp2, igbtTemp3, gateTemp;
    igbtTemp1 = data[0] + (data[1] * 256);
	igbtTemp2 = data[2] + (data[3] * 256);
    igbtTemp3 = data[4] + (data[5] * 256);
    gateTemp = data[6] + (data[7] * 256);
    Logger::debug("IGBT Temps - 1: %d  2: %d  3: %d     Gate Driver: %d    (0.1C)", igbtTemp1, igbtTemp2, igbtTemp3, gateTemp);
    temperatureInverter = igbtTemp1;
    if (igbtTemp2 > temperatureInverter) temperatureInverter = igbtTemp2;
    if (igbtTemp3 > temperatureInverter) temperatureInverter = igbtTemp3;
    if (gateTemp > temperatureInverter) temperatureInverter = gateTemp;
}

void RMSMotorController::handleCANMsgTemperature2(uint8_t *data)
{
    int ctrlTemp, rtdTemp1, rtdTemp2, rtdTemp3;
    ctrlTemp = data[0] + (data[1] * 256);
	rtdTemp1 = data[2] + (data[3] * 256);
    rtdTemp2 = data[4] + (data[5] * 256);
    rtdTemp3 = data[6] + (data[7] * 256);
    Logger::debug("Ctrl Temp: %d  RTD1: %d   RTD2: %d   RTD3: %d    (0.1C)", ctrlTemp, rtdTemp1, rtdTemp2, rtdTemp3);
	temperatureSystem = ctrlTemp;
}

void RMSMotorController::handleCANMsgTemperature3(uint8_t *data)
{
    int rtdTemp4, rtdTemp5, motorTemp, torqueShudder;
    rtdTemp4 = data[0] + (data[1] * 256);
	rtdTemp5 = data[2] + (data[3] * 256);
    motorTemp = data[4] + (data[5] * 256);
    torqueShudder = data[6] + (data[7] * 256);
    Logger::debug("RTD4: %d   RTD5: %d   Motor Temp: %d    Torque Shudder: %d", rtdTemp4, rtdTemp5, motorTemp, torqueShudder);
	temperatureMotor = motorTemp;
}

void RMSMotorController::handleCANMsgAnalogInputs(uint8_t *data)
{
	int analog1, analog2, analog3, analog4;
    analog1 = data[0] + (data[1] * 256);
	analog2 = data[2] + (data[3] * 256);
    analog3 = data[4] + (data[5] * 256);
    analog4 = data[6] + (data[7] * 256);
	Logger::debug("RMS  A1: %d   A2: %d   A3: %d   A4: %d", analog1, analog2, analog3, analog4);
}

void RMSMotorController::handleCANMsgDigitalInputs(uint8_t *data)
{
	//in case it matters:  (1 - 8 not 0 - 7)
	//DI 1 = Forward switch, 2 = Reverse Switch, 3 = Brake Switch, 4 = Regen Disable Switch, 5 = Ignition, 6 = Start 
	uint8_t digInputs = 0;
	for (int i = 0; i < 8; i++)
	{
		if (data[i] == 1) digInputs |= 1 << i;
	}
	Logger::debug("Digital Inputs: %b", digInputs);
}

void RMSMotorController::handleCANMsgMotorPos(uint8_t *data)
{
	int motorAngle, motorSpeed, elecFreq, deltaResolver;
    motorAngle = data[0] + (data[1] * 256);
	motorSpeed = data[2] + (data[3] * 256);
    elecFreq = data[4] + (data[5] * 256);
    deltaResolver = data[6] + (data[7] * 256);
	speedActual = motorSpeed;
	Logger::debug("Angle: %d   Speed: %d   Freq: %d    Delta: %d", motorAngle, motorSpeed, elecFreq, deltaResolver);
}

void RMSMotorController::handleCANMsgCurrent(uint8_t *data)
{
	int phaseCurrentA, phaseCurrentB, phaseCurrentC, busCurrent;
    phaseCurrentA = data[0] + (data[1] * 256);
	phaseCurrentB = data[2] + (data[3] * 256);
    phaseCurrentC = data[4] + (data[5] * 256);
    busCurrent = data[6] + (data[7] * 256);
	dcCurrent = busCurrent;
	acCurrent = phaseCurrentA;
	if (phaseCurrentB > acCurrent) acCurrent = phaseCurrentB;
	if (phaseCurrentC > acCurrent) acCurrent = phaseCurrentC;
	Logger::debug("Phase A: %d    B: %d   C: %d    Bus Current: %d", phaseCurrentA, phaseCurrentB, phaseCurrentC, busCurrent);
}

void RMSMotorController::handleCANMsgVoltage(uint8_t *data)
{
	int dcVoltage, outVoltage, Vd, Vq;
    dcVoltage = data[0] + (data[1] * 256);
	outVoltage = data[2] + (data[3] * 256);
    Vd = data[4] + (data[5] * 256);
    Vq = data[6] + (data[7] * 256);
	Logger::debug("Bus Voltage: %d    OutVoltage: %d   Vd: %d    Vq: %d", dcVoltage, outVoltage, Vd, Vq);
}

void RMSMotorController::handleCANMsgFlux(uint8_t *data)
{
	int fluxCmd, fluxEst, Id, Iq;
    fluxCmd = data[0] + (data[1] * 256);
	fluxEst = data[2] + (data[3] * 256);
    Id = data[4] + (data[5] * 256);
    Iq = data[6] + (data[7] * 256);
	Logger::debug("Flux Cmd: %d  Flux Est: %d   Id: %d    Iq: %d", fluxCmd, fluxEst, Id, Iq);
}

void RMSMotorController::handleCANMsgIntVolt(uint8_t *data)
{
	int volts15, volts25, volts50, volts120;
    volts15 = data[0] + (data[1] * 256);
	volts25 = data[2] + (data[3] * 256);
    volts50 = data[4] + (data[5] * 256);
    volts120 = data[6] + (data[7] * 256);
	Logger::debug("1.5V: %d   2.5V: %d   5.0V: %d    12V: %d", volts15, volts25, volts50, volts120);
}

void RMSMotorController::handleCANMsgIntState(uint8_t *data)
{
	int vsmState, invState, relayState, invRunMode, invActiveDischarge, invCmdMode, invEnable, invLockout, invDirection;
	
	vsmState = data[0] + (data[1] * 256);
	invState = data[2];
	relayState = data[3];
	invRunMode = data[4] & 1;
	invActiveDischarge = data[4] >> 5;
	invCmdMode = data[5];
	isEnabled = data[6] & 1;
	isLockedOut = data[6] >> 7;
	invDirection = data[7];

    switch (vsmState)
	{
    case 0:
	    Logger::debug("VSM Start");
		break;		
    case 1:
	    Logger::debug("VSM Precharge Init");
		break;		
    case 2:
	    Logger::debug("VSM Precharge Active");
		break;		
    case 3:
	    Logger::debug("VSM Precharge Complete");
		break;		
    case 4:
	    Logger::debug("VSM Wait");
		break;		
    case 5:
	    Logger::debug("VSM Ready");
		break;		
    case 6:
	    Logger::debug("VSM Motor Running");
		break;		
    case 7:
	    Logger::debug("VSM Blink Fault Code");
		break;		
    case 14:
	    Logger::debug("VSM Shutdown in process");
		break;		
    case 15:
	    Logger::debug("VSM Recycle power state");
		break;		
    default:
	    Logger::debug("Unknown VSM State!");
		break;				
	}	
	
	switch (invState)
	{
    case 0:
	    Logger::debug("Inv - Power On");
		break;		
    case 1:
	    Logger::debug("Inv - Stop");
		break;		
    case 2:
	    Logger::debug("Inv - Open Loop");
		break;		
    case 3:
	    Logger::debug("Inv - Closed Loop");
		break;		
    case 4:
	    Logger::debug("Inv - Wait");
		break;		
    case 8:
	    Logger::debug("Inv - Idle Run");
		break;		
    case 9:
	    Logger::debug("Inv - Idle Stop");
		break;		
    default:
	    Logger::debug("Internal Inverter State");
		break;				
	}
	
	Logger::debug ("Relay States: %b", relayState);
	
	if (invRunMode) powerMode = modeSpeed;
	else powerMode = modeTorque;
	
	switch (invActiveDischarge)
	{
	case 0:
		Logger::debug("Active Discharge Disabled");
		break;
	case 1:
		Logger::debug("Active Discharge Enabled - Waiting");
		break;
	case 2:
		Logger::debug("Active Discharge Checking Speed");
		break;
	case 3:
		Logger::debug("Active Discharge In Process");
		break;
	case 4:
		Logger::debug("Active Discharge Completed");
		break;		
	}
	
	if (invCmdMode)
	{
		Logger::debug("VSM Mode Active");
		isCANControlled = false;
	}
	else
	{
		Logger::debug("CAN Mode Active");
		isCANControlled = true;
	}
	
	Logger::debug("Enabled: %u    Forward: %u", isEnabled, invDirection);
}

void RMSMotorController::handleCANMsgFaults(uint8_t *data)
{
	uint32_t postFaults, runFaults;
	
	postFaults = data[0] + (data[1] * 256) + (data[2] * 65536ul) + (data[3] * 16777216ul);
	runFaults = data[4] + (data[5] * 256) + (data[6] * 65536ul) + (data[7] * 16777216ul);
	
	//for non-debugging purposes if either of the above is not zero then crap has hit the fan. Register as faulted and quit trying to move
	if (postFaults != 0 || runFaults != 0) faulted = true;
	else faulted = false;
	
	if (postFaults & 1) Logger::error("Desat Fault!");
	if (postFaults & 2) Logger::error("HW Over Current Limit!");
	if (postFaults & 4) Logger::error("Accelerator Shorted!");
	if (postFaults & 8) Logger::error("Accelerator Open!");
	if (postFaults & 0x10) Logger::error("Current Sensor Low!");
	if (postFaults & 0x20) Logger::error("Current Sensor High!");
	if (postFaults & 0x40) Logger::error("Module Temperature Low!");
	if (postFaults & 0x80) Logger::error("Module Temperature High!");
	if (postFaults & 0x100) Logger::error("Control PCB Low Temp!");
	if (postFaults & 0x200) Logger::error("Control PCB High Temp!");
	if (postFaults & 0x400) Logger::error("Gate Drv PCB Low Temp!");
	if (postFaults & 0x800) Logger::error("Gate Drv PCB High Temp!");
	if (postFaults & 0x1000) Logger::error("5V Voltage Low!");
	if (postFaults & 0x2000) Logger::error("5V Voltage High!");
	if (postFaults & 0x4000) Logger::error("12V Voltage Low!");
	if (postFaults & 0x8000) Logger::error("12V Voltage High!");
	if (postFaults & 0x10000ul) Logger::error("2.5V Voltage Low!");
	if (postFaults & 0x20000ul) Logger::error("2.5V Voltage High!");
	if (postFaults & 0x40000ul) Logger::error("1.5V Voltage Low!");
	if (postFaults & 0x80000ul) Logger::error("1.5V Voltage High!");
	if (postFaults & 0x100000ul) Logger::error("DC Bus Voltage High!");
	if (postFaults & 0x200000ul) Logger::error("DC Bus Voltage Low!");
	if (postFaults & 0x400000ul) Logger::error("Precharge Timeout!");
	if (postFaults & 0x800000ul) Logger::error("Precharge Voltage Failure!");
	if (postFaults & 0x1000000ul) Logger::error("EEPROM Checksum Invalid!");
	if (postFaults & 0x2000000ul) Logger::error("EEPROM Data Out of Range!");
	if (postFaults & 0x4000000ul) Logger::error("EEPROM Update Required!");
	if (postFaults & 0x40000000ul) Logger::error("Brake Shorted!");
	if (postFaults & 0x80000000ul) Logger::error("Brake Open!");	
	
	if (runFaults & 1) Logger::error("Motor Over Speed!");
	if (runFaults & 2) Logger::error("Over Current!");
	if (runFaults & 4) Logger::error("Over Voltage!");
	if (runFaults & 8) Logger::error("Inverter Over Temp!");
	if (runFaults & 0x10) Logger::error("Accelerator Shorted!");
	if (runFaults & 0x20) Logger::error("Accelerator Open!");
	if (runFaults & 0x40) Logger::error("Direction Cmd Fault!");
	if (runFaults & 0x80) Logger::error("Inverter Response Timeout!");
	if (runFaults & 0x100) Logger::error("Hardware Desat Error!");
	if (runFaults & 0x200) Logger::error("Hardware Overcurrent Fault!");
	if (runFaults & 0x400) Logger::error("Under Voltage!");
	if (runFaults & 0x800) Logger::error("CAN Cmd Message Lost!");
	if (runFaults & 0x1000) Logger::error("Motor Over Temperature!");
	if (runFaults & 0x10000ul) Logger::error("Brake Input Shorted!");
	if (runFaults & 0x20000ul) Logger::error("Brake Input Open!");
	if (runFaults & 0x40000ul) Logger::error("IGBT A Over Temperature!");
	if (runFaults & 0x80000ul) Logger::error("IGBT B Over Temperature!");
	if (runFaults & 0x100000ul) Logger::error("IGBT C Over Temperature!");
	if (runFaults & 0x200000ul) Logger::error("PCB Over Temperature!");
	if (runFaults & 0x400000ul) Logger::error("Gate Drive 1 Over Temperature!");
	if (runFaults & 0x800000ul) Logger::error("Gate Drive 2 Over Temperature!");
	if (runFaults & 0x1000000ul) Logger::error("Gate Drive 3 Over Temperature!");
	if (runFaults & 0x2000000ul) Logger::error("Current Sensor Fault!");
	if (runFaults & 0x40000000ul) Logger::error("Resolver Not Connected!");
	if (runFaults & 0x80000000ul) Logger::error("Inverter Discharge Active!");
	
}

void RMSMotorController::handleCANMsgTorqueTimer(uint8_t *data)
{
	int cmdTorque, actTorque;
	uint32_t uptime;
	
	cmdTorque = data[0] + (data[1] * 256);
	actTorque = data[2] + (data[3] * 256);
	uptime = data[4] + (data[5] * 256) + (data[6] * 65536ul) + (data[7] * 16777216ul);
	Logger::debug("Torque Cmd: %d   Actual: %d     Uptime: %d", cmdTorque, actTorque, uptime);
	torqueActual = actTorque;
	//torqueCommand = cmdTorque; //should this be here? We set commanded torque and probably shouldn't overwrite here.
}

void RMSMotorController::handleCANMsgModFluxWeaken(uint8_t *data)
{
	int modIdx, fieldWeak, IdCmd, IqCmd;
    modIdx = data[0] + (data[1] * 256);
	fieldWeak = data[2] + (data[3] * 256);
    IdCmd = data[4] + (data[5] * 256);
    IqCmd = data[6] + (data[7] * 256);
	Logger::debug("Mod: %d  Weaken: %d   Id: %d   Iq: %d", modIdx, fieldWeak, IdCmd, IqCmd);
}

void RMSMotorController::handleCANMsgFirmwareInfo(uint8_t *data)
{
	int EEVersion, firmVersion, dateMMDD, dateYYYY;
    EEVersion = data[0] + (data[1] * 256);
	firmVersion = data[2] + (data[3] * 256);
    dateMMDD = data[4] + (data[5] * 256);
    dateYYYY = data[6] + (data[7] * 256);
	Logger::debug("EEVer: %d  Firmware: %d   Date: %d %d", EEVersion, firmVersion, dateMMDD, dateYYYY);
}

void RMSMotorController::handleCANMsgDiagnostic(uint8_t *data)
{
}

void RMSMotorController::handleTick() {

    MotorController::handleTick(); //kick the ball up to papa
	
    if (isCANControlled) sendCmdFrame();   //Send out control message if inverter tells us it's set to CAN control. Otherwise just listen

    if(!online)  //This routine checks to see if we have received any frames from the inverter.  If so, ONLINE would be true and
    {   //we set the RUNNING light on.  If no frames are received for 2 seconds, we set running OFF.
        if (millis()-mss>2000)
        {
            running = false; // We haven't received any frames for over 2 seconds.  Otherwise online would be true.
            mss=millis();   //Reset our 2 second timer
        }
    }
    else running = true;
    online = false;//This flag will be set to true by received frames
}


void RMSMotorController::sendCmdFrame()
{
    RMSMotorControllerConfiguration *config = (RMSMotorControllerConfiguration *)getConfiguration();

    CAN_message_t output;
    output.len = 8;
    output.id = 0xC0;
    output.flags.extended = 0; //standard frame
	//Byte 0-1 = Torque command
	//Byte 2-3 = Speed command (send 0, we don't do speed control)
	//Byte 4 is Direction (0 = CW, 1 = CCW)
	//Byte 5 = Bit 0 is Enable, Bit 1 = Discharge (Discharge capacitors)
	//Byte 6-7 = Commanded Torque Limit (Send as 0 to accept EEPROM parameter unless we're setting the limit really low for some reason such as faulting or a warning)
	
	//Speed set as 0
	output.buf[2] = 0;
	output.buf[3] = 0;
	
	//Torque limit set as 0
	output.buf[6] = 0;
	output.buf[7] = 0;
	
	
    if(operationState == ENABLE && !isLockedOut && selectedGear != NEUTRAL && donePrecharge)
    {
        output.buf[5] = 1;
    }
    else
    {
        output.buf[5] = 0;
    }

    if(selectedGear == DRIVE)
    {
        output.buf[4] = 0;
    }
    else
    {
        output.buf[4] = 1;
    }
    
    torqueRequested = ((throttleRequested * config->torqueMax) / 1000); //Calculate torque request from throttle position x maximum torque
    if(speedActual<config->speedMax) {
        torqueCommand = torqueRequested;   //If actual rpm less than max rpm, add torque command to offset
    }
    else {
        torqueCommand = torqueRequested / 2;   //If at RPM limit, cut torque command in half.
    }
    
    if (torqueRequested < 0) torqueRequested = 0;
    
    Logger::debug("ThrottleRequested: %d     TorqueRequested: %d", throttleRequested, torqueRequested);
	
    output.buf[1] = (torqueCommand & 0xFF00) >> 8;  //Stow torque command in bytes 0 and 1.
    output.buf[0] = (torqueCommand & 0x00FF);
    
    canHandlerEv.sendFrame(output);  //Mail it.

    Logger::debug("CAN Command Frame: %X  %X  %X  %X  %X  %X  %X  %X",output.id, output.buf[0],
                  output.buf[1],output.buf[2],output.buf[3],output.buf[4],
				  output.buf[5],output.buf[6],output.buf[7]);
}

DeviceId RMSMotorController::getId() {
    return (RINEHARTINV);
}

uint32_t RMSMotorController::getTickInterval()
{
    return CFG_TICK_INTERVAL_MOTOR_CONTROLLER;
}

void RMSMotorController::loadConfiguration() {
    RMSMotorControllerConfiguration *config = (RMSMotorControllerConfiguration *)getConfiguration();

    if (!config) {
        config = new RMSMotorControllerConfiguration();
        setConfiguration(config);
    }

    MotorController::loadConfiguration(); // call parent
}

void RMSMotorController::saveConfiguration() {
    MotorController::saveConfiguration();
}

RMSMotorController rmsMC;