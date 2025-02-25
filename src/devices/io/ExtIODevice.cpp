#include "ExtIODevice.h"

ExtIODevice::ExtIODevice() : Device()
{
	numDigitalOutputs = 0;
	numAnalogOutputs = 0;
	numDigitalInputs = 0;
	numAnalogInputs = 0;
	deviceType = DEVICE_IO;
}

void ExtIODevice::setup()
{
	Device::setup();
}

void ExtIODevice::tearDown()
{
	//Device::tearDown();
}

void ExtIODevice::handleMessage(uint32_t msg, void* data)
{
	Device::handleMessage(msg, data);
}

int ExtIODevice::getDigitalOutputCount()
{
	return numDigitalOutputs;
}

int ExtIODevice::getAnalogOutputCount()
{
	return numAnalogOutputs;
}

int ExtIODevice::getDigitalInputCount()
{
	return numDigitalInputs;
}

int ExtIODevice::getAnalogInputCount()
{
	return numAnalogInputs;
}

//a bunch of do nothing implementations of these functions so derived classes that don't support certain
//I/O types and modes can just ignore them

void ExtIODevice::setDigitalOutput(int which, bool hi)
{
	//do nothing if this version gets called
}

bool ExtIODevice::getDigitalOutput(int which)
{
	return false;
}

void ExtIODevice::setAnalogOutput(int which, int value)
{
}

int16_t ExtIODevice::getAnalogOutput(int which)
{
	return 0;
}

bool ExtIODevice::getDigitalInput(int which)
{
	return false;
}

int16_t ExtIODevice::getAnalogInput(int which)
{
	return 0;
}

void ExtIODevice::setLatchingMode(int which, LatchModes::LATCHMODE mode)
{
}

void ExtIODevice::unlockLatch(int which)
{
}

void ExtIODevice::loadConfiguration() {
    ExtIODeviceConfiguration *config = (ExtIODeviceConfiguration *) getConfiguration();

    if (!config) { // as lowest sub-class make sure we have a config object
        config = new ExtIODeviceConfiguration();
        setConfiguration(config);
    }

    Device::loadConfiguration(); // call parent

}

/*
 * Store the current configuration to EEPROM
 */
void ExtIODevice::saveConfiguration() {
    ExtIODeviceConfiguration *config = (ExtIODeviceConfiguration *) getConfiguration();

    Device::saveConfiguration(); // call parent
	prefsHandler->forceCacheWrite();

}
