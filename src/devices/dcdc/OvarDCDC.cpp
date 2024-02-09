#include "OvarDCDC.h"

OvarDCDCController::OvarDCDCController() : DCDCController()
{
    commonName = "Ovar DC-DC Converter";
    shortName = "OvarDCDC";
}

void OvarDCDCController::earlyInit()
{
    prefsHandler = new PrefHandler(OVAR_DCDC);
}

void OvarDCDCController::setup()
{
    //prefsHandler->setEnabledStatus(true);
    tickHandler.detach(this);

    loadConfiguration();
    DCDCController::setup(); // run the parent class version of this function

    OvarDCDCConfiguration *config = (OvarDCDCConfiguration *)getConfiguration();

    ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type                   Min Max Precision Funct
    entry = {"OVARDCDC-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr};
    cfgEntries.push_back(entry);

    setAttachedCANBus(config->canbusNum);

    //watch for the DC/DC status message
    attachedCANBus->attach(this, 0x1806F4D5, 0x1FFFFFFF, true);
    tickHandler.attach(this, CFG_TICK_INTERVAL_DCDC);
    crashHandler.addBreadcrumb(ENCODE_BREAD("OVRDC") + 0);
}

void OvarDCDCController::handleCanFrame(const CAN_message_t &frame)
{
    uint16_t dcdcVoltage = 0;
    uint16_t dcdcAmps = 0;
    int16_t dcdcTemperature = 0;
    uint8_t dcdc_status = 0;
    Logger::debug("Ovar DCDC: %X   %X   %X   %X   %X   %X   %X   %X  %X", frame.id, frame.buf[0],
                  frame.buf[1],frame.buf[2],frame.buf[3],frame.buf[4],
                  frame.buf[5],frame.buf[6],frame.buf[7]);

    if (frame.id == 0x1806F4D5)
    {
	    dcdcVoltage = (frame.buf[0] << 8) + (frame.buf[1]);
	    dcdcAmps = (frame.buf[2] << 8) + (frame.buf[3]);
        dcdcTemperature = frame.buf[4] - 40;
        dcdc_status = frame.buf[5];
        if (dcdc_status & 1) Logger::error("DCDC has failed");
        if (dcdc_status & 2) Logger::error("DCDC temperature abnormal");
        if (dcdc_status & 4) Logger::error("DCDC Input voltage abnormal");
        if (dcdc_status & 8) Logger::error("DCDC output voltage abnormal");
        if (dcdc_status & 16) Logger::error("Comm timeout. Failed!");
        Logger::debug("DCDC    V: %f  A: %f   Status: %u", dcdcVoltage / 10.0f, dcdcAmps / 10.0f, dcdc_status);
	}
}

void OvarDCDCController::handleTick()
{
    DCDCController::handleTick(); //kick the ball up to papa

    sendCmd();   //Send our Delphi voltage control command
}

void OvarDCDCController::sendCmd()
{
    OvarDCDCConfiguration *config = (OvarDCDCConfiguration *)getConfiguration();

    CAN_message_t output;
    output.len = 8;
    output.id = 0x1806D5F4;
    output.flags.extended = 1;
    output.buf[0] = 0;
    output.buf[1] = 0;
    output.buf[2] = 0;
    output.buf[3] = 0;
    output.buf[4] = 0;
    output.buf[5] = 0;
    output.buf[6] = 0;
    output.buf[7] = 0;

    attachedCANBus->sendFrame(output);
    Logger::debug("Ovar DC-DC cmd: %X %X %X %X %X %X %X %X %X",output.id, output.buf[0],
                  output.buf[1],output.buf[2],output.buf[3],output.buf[4],output.buf[5],output.buf[6],output.buf[7]);
    crashHandler.addBreadcrumb(ENCODE_BREAD("OVRDC") + 1);
}

DeviceId OvarDCDCController::getId() {
    return (OVAR_DCDC);
}

uint32_t OvarDCDCController::getTickInterval()
{
    return CFG_TICK_INTERVAL_DCDC;
}

void OvarDCDCController::loadConfiguration() {
    OvarDCDCConfiguration *config = (OvarDCDCConfiguration *)getConfiguration();

    if (!config) {
        config = new OvarDCDCConfiguration();
        setConfiguration(config);
    }

    DCDCController::loadConfiguration(); // call parent

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
}

void OvarDCDCController::saveConfiguration() {
    OvarDCDCConfiguration *config = (OvarDCDCConfiguration *)getConfiguration();

    if (!config) {
        config = new OvarDCDCConfiguration();
        setConfiguration(config);
    }
    prefsHandler->write("CanbusNum", config->canbusNum);
    DCDCController::saveConfiguration();
}

DMAMEM OvarDCDCController ovardc;
