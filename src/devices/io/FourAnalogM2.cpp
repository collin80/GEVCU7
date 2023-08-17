#include "FourAnalogM2.h"
#include "../../sys_io.h"
#include <SPI.h>

//chances are nothing has initialized SPI since it isn't used except by M2 devices or the ESP32 interface. SO, make sure to initialize it.
//This is set REALLY slow right now - 1MHz SPI. The chip is capable of 50MHz. But, no telling if my crummy trace routing job will handle
//that speed. It's probably OK to crank it to 10MHz. But, one DAC update is 24 bits so even at 1Mhz the delay is not long.
SPISettings spi_settings(1000000, MSBFIRST, SPI_MODE0);   

FourAnalogM2::FourAnalogM2()
{	
	numDigitalOutputs = 0;
	numAnalogOutputs = 4; //this chip has four DAC outputs on it. 0-5v each
	numDigitalInputs = 0;
	numAnalogInputs = 0;

	for (int i = 0; i < 4; i++)
	{
		values[i] = 0;
	}

	commonName = "4 Output Analog M2";
    shortName = "4ANA-M2";
}

void FourAnalogM2::earlyInit()
{
    prefsHandler = new PrefHandler(FOURANALOGM2);
}

void FourAnalogM2::setup()
{
    crashHandler.addBreadcrumb(ENCODE_BREAD("4ANAO") + 0);
	ExtIODevice::setup();

    SPI.begin();
    pinMode(10, OUTPUT);
    digitalWrite(10, HIGH);

	Logger::debug(FOURANALOGM2, "Now setting up.");

    loadConfiguration();

    //FourAnaM2DeviceConfiguration *config = (FourAnaM2DeviceConfiguration *)getConfiguration();

    //ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type                   Min Max Precision Funct
    //entry = {"POWERKEY-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr};
    //cfgEntries.push_back(entry);
		
	systemIO.installExtendedIO(this);
    //setAnalogOutput(0, 64);
    //setAnalogOutput(1, 128);
    //setAnalogOutput(2, 256);
    //setAnalogOutput(3, 512);
}


void FourAnalogM2::handleMessage(uint32_t msgType, void* data)
{
	ExtIODevice::handleMessage(msgType, data);
}

DeviceId FourAnalogM2::getId()
{
	return FOURANALOGM2;
}

void FourAnalogM2::setAnalogOutput(int which, int value)
{
	if (which < 0) return;
	if (which > 3) return;
	if (value < 0) return;
	if (value > 1023) return;
    crashHandler.addBreadcrumb(ENCODE_BREAD("4ANAO") + 1);
    Logger::debug("AnalogOut %i with value %i", which, value);

    uint8_t data[3];
    //        write DAC     Which DAC
    data[0] = (0x3 << 4) + (1 << which);
    data[1] = (value >> 2); //top 8 bits
    data[2] = (value & 3) << 6; //bottom 2 bits

    digitalWrite(10, LOW); //select DAC
    SPI.beginTransaction(spi_settings);
    SPI.transfer(data, 3); //send all three bytes in one shot
    SPI.endTransaction();
    digitalWrite(10, HIGH); //deselect DAC chip and let it update
}

int16_t FourAnalogM2::getAnalogOutput(int which)
{
	if (which >= 0 && which < 4)
		return values[which];
	return 0;
}

void FourAnalogM2::loadConfiguration() {
    FourAnaM2DeviceConfiguration *config = (FourAnaM2DeviceConfiguration *)getConfiguration();

    if (!config) {
        config = new FourAnaM2DeviceConfiguration();
        setConfiguration(config);
    }

    ExtIODevice::loadConfiguration(); // call parent
}

void FourAnalogM2::saveConfiguration() {
    FourAnaM2DeviceConfiguration *config = (FourAnaM2DeviceConfiguration *)getConfiguration();

    if (!config) {
        config = new FourAnaM2DeviceConfiguration();
        setConfiguration(config);
    }
    ExtIODevice::saveConfiguration();
}

FourAnalogM2 four_ana;