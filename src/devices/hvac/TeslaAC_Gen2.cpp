#include "TeslaAC_Gen2.h"

TeslaACGen2Controller::TeslaACGen2Controller() : HVACController()
{
    commonName = "Tesla Model S Gen2 AC";
    shortName = "TSLAGEN2AC";
    canCool = true;
    pid = nullptr;
    deviceId = TESLA_AC_GEN2;
}

void TeslaACGen2Controller::handleCanFrame(const CAN_message_t &frame)
{
    int16_t inverterTemperature;
    uint16_t statusBits;
    uint8_t compressorState;

    setAlive();

    switch (frame.id)
    {
    case 0x223: //state of the compressor
        //not using a lot of these currently but here are all the definitions
        //uint16_t rpm = frame.buf[0] + (frame.buf[1] * 256);
        //float duty = (frame.buf[2] + (frame.buf[3] * 256)) * 0.1f;
        inverterTemperature = frame.buf[4] - 40;
        statusBits = (frame.buf[5] + (frame.buf[6] * 256));
        isFaulted = (statusBits != 0);
        isReady = (frame.buf[7] & 0x80);
        //compressorState = frame.buf[7] & 0x0F;
        //then, decode the status bits just for giggles
        if (statusBits & 1)
        {
            Logger::debug("HV over voltage!");
        }
        if (statusBits & 2)
        {
            Logger::debug("HV under voltage!");
        }
        if (statusBits & 4)
        {
            Logger::debug("Compressor over temperature!");
        }
        if (statusBits & 8)
        {
            Logger::debug("Compressor too cold!");
        }
        if (statusBits & 0x10)
        {
            Logger::debug("CAN command timeout!");
        }
        if (statusBits & 0x20)
        {
            Logger::debug("Compressor over current!");
        }
        if (statusBits & 0x40)
        {
            Logger::debug("Current sensor fault!");
        }
        if (statusBits & 0x80)
        {
            Logger::debug("Compressor failed to start!");
        }
        if (statusBits & 0x100)
        {
            Logger::debug("Compressor voltage saturation error!");
        }
        if (statusBits & 0x200)
        {
            Logger::debug("Compressor has a short circuit!");
        }
        if (statusBits & 0x400)
        {
            Logger::debug("Compressor repeatedly going over current!");
        }
        break;
    case 0x233: //HV status, voltage, current, wattage
        //only really care about the wattage so we can report it
        wattage = frame.buf[5] + (frame.buf[6] * 256);
        //these things can be decoded
        //float hvVoltage = (frame.buf[0] + (frame.buf[1] * 256)) * 0.1f;
        //float hvCurrent = (frame.buf[3] + (frame.buf[4] * 256)) * 0.1f;
        break;
    }
    Logger::debug("TeslaACGen2 msg: %X", frame.id);
    Logger::debug("TeslaACGen2 data: %X%X%X%X%X%X%X%X", frame.buf[0],frame.buf[1],frame.buf[2],frame.buf[3],frame.buf[4],frame.buf[5],frame.buf[6],frame.buf[7]);
}

void TeslaACGen2Controller::setup()
{
    //prefsHandler->setEnabledStatus(true);
    tickHandler.detach(this);

    loadConfiguration();
    HVACController::setup(); // run the parent class version of this function

    TeslaACGen2Configuration *config = (TeslaACGen2Configuration *)getConfiguration();

    targetTempC = config->targetTemperature;

    //create our PID controller. Has to be done after the getConfiguration above since we use the PID params
    //may need reverse instead of direct? AC wants to lower the temperature and lowering the temp means increasing duty
    //but, unsure of proper direction???
    pid = new PID(&currentTemperature, &targetDuty, &targetTempC, config->kP, config->kI, config->kD, DIRECT);
    pid->SetMode(AUTOMATIC);
    pid->SetOutputLimits(0, 1000); //0 - 100% in 0.1 increments

    ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type                   Min Max Precision Funct
    entry = {"TESLAG2AC-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TESLAG2AC-MAXPOW", "Maximum allowable wattage draw of compressor", &config->maxPower, CFG_ENTRY_VAR_TYPE::UINT16, 0, 8000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TESLAG2AC-KP", "Proportional value of PID controller", &config->kP, CFG_ENTRY_VAR_TYPE::FLOAT, 0, 80.0, 2, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TESLAG2AC-KI", "Integral value of PID controller", &config->kI, CFG_ENTRY_VAR_TYPE::FLOAT, 0, 80.0, 2, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TESLAG2AC-KD", "Differential value of PID controller", &config->kD, CFG_ENTRY_VAR_TYPE::FLOAT, 0, 80.0, 2, nullptr};
    cfgEntries.push_back(entry);

    setAttachedCANBus(config->canbusNum);

    setAlive();

    attachedCANBus->attach(this, 0x203, 0x7CF, false); //need 0x223 and 0x233
    tickHandler.attach(this, CFG_TICK_INTERVAL_COMPRESSOR);

    crashHandler.addBreadcrumb(ENCODE_BREAD("TG2AC") + 0);
}


void TeslaACGen2Controller::handleTick()
{
    HVACController::handleTick(); //kick the ball up to papa

    checkAlive(4000);

    TeslaACGen2Configuration *config = (TeslaACGen2Configuration *)getConfiguration();
    targetTempC = config->targetTemperature;

    sendCmd();   //Send our compressor command message
}

void TeslaACGen2Controller::sendCmd()
{
    TeslaACGen2Configuration *config = (TeslaACGen2Configuration *)getConfiguration();

    CAN_message_t output;
    output.len = 8;
    output.id = 0x28A;
    output.flags.extended = 0; //standard frame
    //the big question is what the target duty should be. This will likely be best done
    //right now it's hard coded. Bad idea.
    if (isReady)
    {
        pid->Compute();
        if (targetDuty < 0.0) targetDuty = 0.0;
        uint16_t duty = (targetDuty * 10);
        output.buf[0] = (duty & 0xFF);
        output.buf[1] = (duty >> 8) & 0xFF;
        output.buf[2] = (config->maxPower & 0xFF);
        output.buf[3] = (config->maxPower >> 8) & 0xFF;
        output.buf[4] = 0; //compressor reset value. Doesn't seem to need to be anything other than 0 normally
        output.buf[5] = 1; //1 = enable compressor 0 = turn it off. This should also be tuned.
        output.buf[6] = 0;
        output.buf[7] = 0;
    }
    else
    {
        output.buf[0] = 0;
        output.buf[1] = 0;
        output.buf[2] = (config->maxPower & 0xFF);
        output.buf[3] = (config->maxPower >> 8) & 0xFF;
        output.buf[4] = 0; //compressor reset value. Doesn't seem to need to be anything other than 0 normally
        output.buf[5] = 0; //1 = enable compressor 0 = turn it off. This should also be tuned.
        output.buf[6] = 0;
        output.buf[7] = 0;
    }

    attachedCANBus->sendFrame(output);
    Logger::debug("Tesla A/C cmd: %X %X %X %X %X %X %X %X %X ",output.id, output.buf[0],
                  output.buf[1],output.buf[2],output.buf[3],output.buf[4],output.buf[5],output.buf[6],output.buf[7]);
    crashHandler.addBreadcrumb(ENCODE_BREAD("TG2AC") + 1);
}

uint32_t TeslaACGen2Controller::getTickInterval()
{
    return CFG_TICK_INTERVAL_COMPRESSOR;
}

void TeslaACGen2Controller::loadConfiguration() {
    TeslaACGen2Configuration *config = (TeslaACGen2Configuration *)getConfiguration();

    if (!config) {
        config = new TeslaACGen2Configuration();
        setConfiguration(config);
    }

    HVACController::loadConfiguration(); // call parent

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
    prefsHandler->read("MaxPower", &config->maxPower, 4500);
    prefsHandler->read("kP", &config->kP, 1.0);
    prefsHandler->read("kI", &config->kI, 0.0);
    prefsHandler->read("kD", &config->kD, 0.0);
}

void TeslaACGen2Controller::saveConfiguration() {
    TeslaACGen2Configuration *config = (TeslaACGen2Configuration *)getConfiguration();

    if (!config) {
        config = new TeslaACGen2Configuration();
        setConfiguration(config);
    }
    prefsHandler->write("CanbusNum", config->canbusNum);
    prefsHandler->write("MaxPower", config->maxPower);
    prefsHandler->write("kP", config->kP);
    prefsHandler->write("kI", config->kI);
    prefsHandler->write("kD", config->kD);
    HVACController::saveConfiguration();
}

DMAMEM TeslaACGen2Controller teslaGen2AC;
