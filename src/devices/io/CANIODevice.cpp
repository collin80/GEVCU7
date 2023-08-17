#include "CANIODevice.h"

CANIODevice::CANIODevice()
{
	numDigitalOutputs = 0;
	numAnalogOutputs = 0;
	numDigitalInputs = 0;
	numAnalogInputs = 0;
}

void CANIODevice::setup()
{
	ExtIODevice::setup();
}

void CANIODevice::tearDown()
{
	//ExtIODDevice::tearDown();
}

void CANIODevice::handleCanFrame(const CAN_message_t &frame)
{
}

void CANIODevice::handleMessage(uint32_t msg, void* data)
{
	//ExtIODDevice::handleMessage(msg, data);
}

void CANIODevice::loadConfiguration() {
    CanIODeviceConfiguration *config = (CanIODeviceConfiguration *) getConfiguration();

    if (!config) { // as lowest sub-class make sure we have a config object
        config = new CanIODeviceConfiguration();
        setConfiguration(config);
    }

    //ExtIODDevice::loadConfiguration(); // call parent

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
}

/*
 * Store the current configuration to EEPROM
 */
void CANIODevice::saveConfiguration() {
    CanIODeviceConfiguration *config = (CanIODeviceConfiguration *) getConfiguration();

    //ExtIODDevice::saveConfiguration(); // call parent

    prefsHandler->write("CanbusNum", config->canbusNum);
    prefsHandler->saveChecksum();
}
