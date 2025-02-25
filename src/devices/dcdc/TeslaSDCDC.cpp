#include "TeslaSDCDC.h"

TSDCDCController::TSDCDCController() : DCDCController()
{
    commonName = "Tesla Model S DC-DC";
    shortName = "TS-DCDC";
    deviceId = TESLA_S_DCDC;
}

/*
bytes 0-1 = fault flags:
    bit 0 = Heater Short
    bit 1 = over temperature
    bit 2 = output under voltage
    bit 3 = bias fault
    bit 4 = input not ok
    bit 5 = output over voltage
    bit 6 = output current limited
    bit 7 = heater open fault
    bit 8 = coolant request (huh?)
    bit 9 = current thermal limit
    bit 10 = output voltage regulation error
    bit 11 = calibration factor CRC error
byte 2 = inlet temperature (0.5C scale, -40 offset)
byte 3 = input power (in 16 watt increment)
byte 4 = dc output current (in amps)
byte 5 = dc output voltage in tenths of a volt
*/
void TSDCDCController::handleCanFrame(const CAN_message_t &frame)
{
    Logger::debug("TS-DCDC msg: %X", frame.id);
    Logger::debug("TS-DCDC data: %X%X%X%X%X%X%X%X", frame.buf[0],frame.buf[1],frame.buf[2],frame.buf[3],frame.buf[4],frame.buf[5],frame.buf[6],frame.buf[7]);
    if (frame.id == 0x210)
    {
        setAlive();
        outputVoltage = frame.buf[5] * 0.1;
        outputCurrent = frame.buf[4];
        deviceTemperature = (frame.buf[2] * 0.5) - 40;
        if ((frame.buf[0] == 0) && (frame.buf[1] == 0))
        {
            isEnabled = true;
            isFaulted = false;
        } 
        else
        {
            isFaulted = true;
            isEnabled = false;
            if (frame.buf[0] & 1) faultHandler.raiseFault(getId(), DEVICE_HARDWARE_FAULT);
            if (frame.buf[0] & 2) faultHandler.raiseFault(getId(), DEVICE_OVER_TEMP);
            if (frame.buf[0] & 4) faultHandler.raiseFault(getId(), DCDC_FAULT_OUTPUTV);
            if (frame.buf[0] & 8) faultHandler.raiseFault(getId(), DEVICE_HARDWARE_FAULT);
            if (frame.buf[0] & 0x10) faultHandler.raiseFault(getId(), DCDC_FAULT_INPUTV);
            if (frame.buf[0] & 0x20) faultHandler.raiseFault(getId(), DCDC_FAULT_OUTPUTV);
            if (frame.buf[0] & 0x40) faultHandler.raiseFault(getId(), DCDC_FAULT_OUTPUTA);
            if (frame.buf[0] & 0x80) faultHandler.raiseFault(getId(), DEVICE_HARDWARE_FAULT);
            if (frame.buf[1] & 1) faultHandler.raiseFault(getId(), DEVICE_HARDWARE_FAULT);
            if (frame.buf[1] & 2) faultHandler.raiseFault(getId(), DEVICE_OVER_TEMP);
            if (frame.buf[1] & 4) faultHandler.raiseFault(getId(), DCDC_FAULT_OUTPUTV);
            if (frame.buf[1] & 8) faultHandler.raiseFault(getId(), DEVICE_HARDWARE_FAULT);
        }
    }
}

void TSDCDCController::earlyInit()
{
    prefsHandler = new PrefHandler(TESLA_S_DCDC);
}

void TSDCDCController::setup()
{
    //prefsHandler->setEnabledStatus(true);
    tickHandler.detach(this);

    loadConfiguration();
    DCDCController::setup(); // run the parent class version of this function

    TSDCDCConfiguration *config = (TSDCDCConfiguration *)getConfiguration();

    ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type                   Min Max Precision Funct
    entry = {"TSDCDC-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr};
    cfgEntries.push_back(entry);

    setAttachedCANBus(config->canbusNum);

    setAlive();

    attachedCANBus->attach(this, 0x210, 0x7ff, false);
    tickHandler.attach(this, CFG_TICK_INTERVAL_DCDC);
    crashHandler.addBreadcrumb(ENCODE_BREAD("TSDCC") + 0);
}

void TSDCDCController::handleTick()
{
    DCDCController::handleTick(); //kick the ball up to papa

    checkAlive(4000);

    sendCmd();   //Send our Delphi voltage control command
}

void TSDCDCController::sendCmd()
{
    TSDCDCConfiguration *config = (TSDCDCConfiguration *)getConfiguration();

    CAN_message_t output;
    output.len = 3;
    output.id = 0x3D8;
    output.flags.extended = 0; //standard frame
    uint16_t vCmd = ((uint16_t)(config->targetLowVoltage- 9.0) * 146.0);
    output.buf[0] = (vCmd & 0xFF);
    output.buf[1] = ((vCmd >> 8) & 3) | 4; //the 4 is the enable bit
    output.buf[2] = 0; //not used

    attachedCANBus->sendFrame(output);
    Logger::debug("Tesla S DC-DC cmd: %X %X %X ",output.id, output.buf[0], output.buf[1],output.buf[2]);
    crashHandler.addBreadcrumb(ENCODE_BREAD("TSDCC") + 1);
}

uint32_t TSDCDCController::getTickInterval()
{
    return CFG_TICK_INTERVAL_DCDC;
}

void TSDCDCController::loadConfiguration() {
    TSDCDCConfiguration *config = (TSDCDCConfiguration *)getConfiguration();

    if (!config) {
        config = new TSDCDCConfiguration();
        setConfiguration(config);
    }

    DCDCController::loadConfiguration(); // call parent

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
}

void TSDCDCController::saveConfiguration() {
    TSDCDCConfiguration *config = (TSDCDCConfiguration *)getConfiguration();

    if (!config) {
        config = new TSDCDCConfiguration();
        setConfiguration(config);
    }
    prefsHandler->write("CanbusNum", config->canbusNum);
    DCDCController::saveConfiguration();
}

DMAMEM TSDCDCController tsdcdc;
