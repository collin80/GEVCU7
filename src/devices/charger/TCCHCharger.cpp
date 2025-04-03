#include "TCCHCharger.h"

TCCHChargerController::TCCHChargerController() : ChargeController()
{
    commonName = "TCCH or Ovar HV Charger";
    shortName = "TCCHCHGR";
    deviceId = TCCH_CHARGER;
}

void TCCHChargerController::setup()
{
    //prefsHandler->setEnabledStatus(true);
    tickHandler.detach(this);

    loadConfiguration();
    ChargeController::setup(); // run the parent class version of this function

    ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type                   Min Max Precision Funct
    entry = {"TCCH-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TCCH-COMMVER", "Set communications version (0 or 1)", &config->commVersion, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);

    setAttachedCANBus(config->canbusNum);

    setAlive();

    //watch for the charger status message
    attachedCANBus->attach(this, 0x18FF50E5, 0x1FFFFFFF, true);
    tickHandler.attach(this, CFG_TICK_INTERVAL_TCCH);
    crashHandler.addBreadcrumb(ENCODE_BREAD("TCCHC") + 0);
}

void TCCHChargerController::handleCanFrame(const CAN_message_t &frame)
{
    uint16_t currentVoltage = 0;
    uint16_t currentAmps = 0;
    uint8_t status = 0;
    Logger::debug("TCCH msg: %X   %X   %X   %X   %X   %X   %X   %X  %X", frame.id, frame.buf[0],
                  frame.buf[1],frame.buf[2],frame.buf[3],frame.buf[4],
                  frame.buf[5],frame.buf[6],frame.buf[7]);

    if (frame.id == 0x18FF50E5)
    {
        setAlive();
	    currentVoltage = (frame.buf[0] << 8) + (frame.buf[1]);
	    currentAmps = (frame.buf[2] << 8) + (frame.buf[3]);
        if (config->commVersion == 1)
        {
            status = frame.buf[4];
            if (status & 1) faultHandler.raiseFault(getId(), DEVICE_HARDWARE_FAULT);
            if (status & 2) faultHandler.raiseFault(getId(), DEVICE_OVER_TEMP);
            if (status & 4) faultHandler.raiseFault(getId(), CHARGER_FAULT_INPUTV);
            if (status & 8) faultHandler.raiseFault(getId(), CHARGER_FAULT_INPUTV);
            if (status & 0x10) faultHandler.raiseFault(getId(), CHARGER_FAULT_OUTPUTV);
            if (status & 0x20) faultHandler.raiseFault(getId(), CHARGER_FAULT_OUTPUTV);
            if (status & 0x40) faultHandler.raiseFault(getId(), CHARGER_FAULT_OUTPUTA);
            if (status & 0x80) faultHandler.raiseFault(getId(), CHARGER_FAULT_OUTPUTA);
            status = frame.buf[5];
            if (status & 1) faultHandler.raiseFault(getId(), COMM_TIMEOUT);
            uint8_t workingStatus = (status >> 1) & 3;
            if (workingStatus == 0) faultHandler.raiseFault(getId(), GENERAL_FAULT);
            //if it equals 2 or 3 then we're stopped and that might be OK so no fault
            //bit 3 is completion of init. Should key on that and not try to command power until it's OK
            //bit 4 is fan status (0 = stopped 1 = request to run)
            //bit 5 is cooling pump on/off
            status = frame.buf[6]; //all about charge port condition
            uint8_t ccState = status & 3;
            //bit 2 is CP signal state (0 = nothing 1 = valid)
            if (status & 8) faultHandler.raiseFault(getId(), DEVICE_OVER_TEMP);
            uint8_t lockState = (status >> 4) & 0x7;
            //bit 7 is S2 switch control status (0 = off 1 = on)
            deviceTemperature = frame.buf[7] - 40.0f;
        }
        else
        {
            status = frame.buf[4];
            if (status & 1) faultHandler.raiseFault(getId(), DEVICE_HARDWARE_FAULT);//Logger::error("Hardware failure of charger");
            if (status & 2) faultHandler.raiseFault(getId(), DEVICE_OVER_TEMP);//Logger::error("Charger over temperature!");
            if (status & 4) faultHandler.raiseFault(getId(), CHARGER_FAULT_INPUTV);//Logger::error("Input voltage out of spec!");
            if (status & 8) faultHandler.raiseFault(getId(), CHARGER_FAULT_OUTPUTV);//Logger::error("Charger can't detect proper battery voltage");
            if (status & 16) faultHandler.raiseFault(getId(), COMM_TIMEOUT);//Logger::error("Comm timeout. Failed!");
        }
        Logger::debug("Charger    V: %f  A: %f   Status: %u", currentVoltage / 10.0f, currentAmps / 10.0f, frame.buf[4]);

        //these two are part of the base class and will automatically be shown to interested parties
        outputVoltage = currentVoltage / 10.0f;
        outputCurrent = currentAmps / 10.0f;
    }
}

void TCCHChargerController::handleTick()
{
    ChargeController::handleTick(); //kick the ball up to papa

    checkAlive(4000);

    sendCmd();   //Send our Delphi voltage control command
}

void TCCHChargerController::sendCmd()
{
    CAN_message_t output;
    output.len = 8;
    output.id = 0x1806E5F4;
    output.flags.extended = 1;

    uint16_t vOutput = config->targetUpperVoltage * 10;
    uint16_t cOutput = config->targetCurrentLimit * 10;

    output.buf[0] = (vOutput >> 8);
    output.buf[1] = (vOutput & 0xFF);
    output.buf[2] = (cOutput >> 8);
    output.buf[3] = (cOutput & 0xFF);
    output.buf[4] = 0; // 0 = start charging 1 = close output (!?!) 2 = charge end (go to sleep)
    output.buf[5] = 0; // 0 = charging mode 1 = battery heating mode
    output.buf[6] = 0; //unused
    output.buf[7] = 0; //unused

    attachedCANBus->sendFrame(output);
    Logger::debug("TCCH Charger cmd: %X %X %X %X %X %X %X %X %X ",output.id, output.buf[0],
                  output.buf[1],output.buf[2],output.buf[3],output.buf[4],output.buf[5],output.buf[6],output.buf[7]);
    crashHandler.addBreadcrumb(ENCODE_BREAD("TCCHC") + 1);
}

uint32_t TCCHChargerController::getTickInterval()
{
    return CFG_TICK_INTERVAL_TCCH;
}

void TCCHChargerController::loadConfiguration() {
    config = (TCCHChargerConfiguration *)getConfiguration();

    if (!config) {
        config = new TCCHChargerConfiguration();
        setConfiguration(config);
    }

    ChargeController::loadConfiguration(); // call parent

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
    prefsHandler->read("CommVer", &config->commVersion, 1);
}

void TCCHChargerController::saveConfiguration() {
    config = (TCCHChargerConfiguration *)getConfiguration();

    if (!config) {
        config = new TCCHChargerConfiguration();
        setConfiguration(config);
    }
    prefsHandler->write("CanbusNum", config->canbusNum);
    prefsHandler->write("CommVer", config->commVersion);
    ChargeController::saveConfiguration();
}

DMAMEM TCCHChargerController tcch;
