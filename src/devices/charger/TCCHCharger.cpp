#include "TCCHCharger.h"

TCCHChargerController::TCCHChargerController() : ChargeController()
{
    commonName = "TCCH or Ovar HV Charger";
    shortName = "TCCHCHGR";
}

void TCCHChargerController::earlyInit()
{
    prefsHandler = new PrefHandler(TCCH_CHARGER);
}

void TCCHChargerController::setup()
{
    //prefsHandler->setEnabledStatus(true);
    tickHandler.detach(this);

    loadConfiguration();
    ChargeController::setup(); // run the parent class version of this function

    TCCHChargerConfiguration *config = (TCCHChargerConfiguration *)getConfiguration();

    ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type                   Min Max Precision Funct
    entry = {"TCCH-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr};
    cfgEntries.push_back(entry);

    setAttachedCANBus(config->canbusNum);

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
	    currentVoltage = (frame.buf[0] << 8) + (frame.buf[1]);
	    currentAmps = (frame.buf[2] << 8) + (frame.buf[3]);
        status = frame.buf[4];
        if (status & 1) Logger::error("Hardware failure of charger");
        if (status & 2) Logger::error("Charger over temperature!");
        if (status & 4) Logger::error("Input voltage out of spec!");
        if (status & 8) Logger::error("Charger can't detect proper battery voltage");
        if (status & 16) Logger::error("Comm timeout. Failed!");
        Logger::debug("Charger    V: %f  A: %f   Status: %u", currentVoltage / 10.0f, currentAmps / 10.0f, status);

        //these two are part of the base class and will automatically be shown to interested parties
        outputVoltage = currentVoltage / 10.0f;
        outputCurrent = currentAmps / 10.0f;
    }
}

void TCCHChargerController::handleTick()
{
    ChargeController::handleTick(); //kick the ball up to papa

    sendCmd();   //Send our Delphi voltage control command
}

void TCCHChargerController::sendCmd()
{
    TCCHChargerConfiguration *config = (TCCHChargerConfiguration *)getConfiguration();

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
    output.buf[4] = 0; // 0 = start charging
    output.buf[5] = 0; //unused
    output.buf[6] = 0; //unused
    output.buf[7] = 0; //unused

    attachedCANBus->sendFrame(output);
    Logger::debug("TCCH Charger cmd: %X %X %X %X %X %X %X %X %X ",output.id, output.buf[0],
                  output.buf[1],output.buf[2],output.buf[3],output.buf[4],output.buf[5],output.buf[6],output.buf[7]);
    crashHandler.addBreadcrumb(ENCODE_BREAD("TCCHC") + 1);
}

DeviceId TCCHChargerController::getId() {
    return (TCCH_CHARGER);
}

uint32_t TCCHChargerController::getTickInterval()
{
    return CFG_TICK_INTERVAL_TCCH;
}

void TCCHChargerController::loadConfiguration() {
    TCCHChargerConfiguration *config = (TCCHChargerConfiguration *)getConfiguration();

    if (!config) {
        config = new TCCHChargerConfiguration();
        setConfiguration(config);
    }

    ChargeController::loadConfiguration(); // call parent

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
}

void TCCHChargerController::saveConfiguration() {
    TCCHChargerConfiguration *config = (TCCHChargerConfiguration *)getConfiguration();

    if (!config) {
        config = new TCCHChargerConfiguration();
        setConfiguration(config);
    }
    prefsHandler->write("CanbusNum", config->canbusNum);
    ChargeController::saveConfiguration();
}

TCCHChargerController tcch;
