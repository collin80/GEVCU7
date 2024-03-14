#include "DelphiDCDC.h"

DelphiDCDCController::DelphiDCDCController() : DCDCController()
{
    commonName = "Delphi DC-DC Converter";
    shortName = "DelphiDCDC";
}



void DelphiDCDCController::handleCanFrame(const CAN_message_t &frame)
{
    setAlive();
    Logger::debug("DelphiDCDC msg: %X", frame.id);
    Logger::debug("DelphiDCDC data: %X%X%X%X%X%X%X%X", frame.buf[0],frame.buf[1],frame.buf[2],frame.buf[3],frame.buf[4],frame.buf[5],frame.buf[6],frame.buf[7]);
}

void DelphiDCDCController::earlyInit()
{
    prefsHandler = new PrefHandler(DELPHI_DCDC);
}

void DelphiDCDCController::setup()
{
    //prefsHandler->setEnabledStatus(true);
    tickHandler.detach(this);

    loadConfiguration();
    DCDCController::setup(); // run the parent class version of this function

    DelphiDCDCConfiguration *config = (DelphiDCDCConfiguration *)getConfiguration();

    ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type                   Min Max Precision Funct
    entry = {"DELPHIDCDC-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr};
    cfgEntries.push_back(entry);

    setAttachedCANBus(config->canbusNum);

    setAlive();

    attachedCANBus->attach(this, 0x1D5, 0x7ff, false);
    //Watch for 0x1D5 messages from Delphi converter
    tickHandler.attach(this, CFG_TICK_INTERVAL_DCDC);
    crashHandler.addBreadcrumb(ENCODE_BREAD("DELPH") + 0);
}


void DelphiDCDCController::handleTick()
{
    DCDCController::handleTick(); //kick the ball up to papa

    checkAlive(4000);

    sendCmd();   //Send our Delphi voltage control command
}


/*
1D7 08 80 77 00 00 00 00 00 00
For 13.0 vdc output.

1D7 08 80 8E 00 00 00 00 00 00
For 13.5 vdc output.

To request 14.0 vdc, the message was:
1D7 08 80 A5 00 00 00 00 00 00

This gives a minimum voltage of 10.4 and each increase of 1 to byte 2 is 0.02173913 more volts.
This still allows for a max output of 16v which is way too much. So, it's a sufficient range for
12v output it seems like.
*/

void DelphiDCDCController::sendCmd()
{
    DelphiDCDCConfiguration *config = (DelphiDCDCConfiguration *)getConfiguration();

    CAN_message_t output;
    output.len = 8;
    output.id = 0x1D7;
    output.flags.extended = 0; //standard frame
    output.buf[0] = 0x80;
    output.buf[1] = (config->targetLowVoltage > 0.0f)?((config->targetLowVoltage - 10.413) / 0.02173913):0;
    output.buf[2] = 0;
    output.buf[3] = 0;
    output.buf[4] = 0;
    output.buf[5] = 0;
    output.buf[6] = 0;
    output.buf[7] = 0x00;

    attachedCANBus->sendFrame(output);
    timestamp();
    Logger::debug("Delphi DC-DC cmd: %X %X %X %X %X %X %X %X %X  %d:%d:%d.%d",output.id, output.buf[0],
                  output.buf[1],output.buf[2],output.buf[3],output.buf[4],output.buf[5],output.buf[6],output.buf[7], hours, minutes, seconds, milliseconds);
    crashHandler.addBreadcrumb(ENCODE_BREAD("DELPH") + 1);
}

DeviceId DelphiDCDCController::getId() {
    return (DELPHI_DCDC);
}

uint32_t DelphiDCDCController::getTickInterval()
{
    return CFG_TICK_INTERVAL_DCDC;
}

void DelphiDCDCController::loadConfiguration() {
    DelphiDCDCConfiguration *config = (DelphiDCDCConfiguration *)getConfiguration();

    if (!config) {
        config = new DelphiDCDCConfiguration();
        setConfiguration(config);
    }

    DCDCController::loadConfiguration(); // call parent

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
}

void DelphiDCDCController::saveConfiguration() {
    DelphiDCDCConfiguration *config = (DelphiDCDCConfiguration *)getConfiguration();

    if (!config) {
        config = new DelphiDCDCConfiguration();
        setConfiguration(config);
    }
    prefsHandler->write("CanbusNum", config->canbusNum);
    DCDCController::saveConfiguration();
}

void DelphiDCDCController::timestamp()
{
    milliseconds = (int) (millis()/1) %1000 ;
    seconds = (int) (millis() / 1000) % 60 ;
    minutes = (int) ((millis() / (1000*60)) % 60);
    hours   = (int) ((millis() / (1000*60*60)) % 24);
}

DMAMEM DelphiDCDCController deldcdc;
