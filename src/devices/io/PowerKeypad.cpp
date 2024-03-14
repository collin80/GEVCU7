#include "Powerkeypad.h"
#include "../../sys_io.h"

PowerkeyPad::PowerkeyPad(void)
{	
	numDigitalOutputs = 0;
	numAnalogOutputs = 12; //hard coded for powerkey pro 2600 which as 12 buttons. Used to run the LED lights
	numDigitalInputs = 12; //hard coded for powerkey pro 2600 which has 12 buttons. There are other units with different # of buttons
	numAnalogInputs = 0;
	deviceID = 0x15;

	for (int i = 0; i < numDigitalInputs; i++)
	{
		buttonState[i] = false;
		actualState[i] = false;
		LEDState[i] = LED::OFF;
		latchState[i] = LatchModes::NO_LATCHING;
		toggleState[i] = LED::OFF;
	}

	commonName = "PowerKey Pro 2600";
    shortName = "PowerKey";
}

void PowerkeyPad::earlyInit()
{
    prefsHandler = new PrefHandler(POWERKEYPRO);
}

void PowerkeyPad::setup()
{
    crashHandler.addBreadcrumb(ENCODE_BREAD("PWRKY") + 0);
	CANIODevice::setup();

	Logger::debug(POWERKEYPRO, "Now setting up.");

    loadConfiguration();

    PowerKPCANIODeviceConfiguration *config = (PowerKPCANIODeviceConfiguration *)getConfiguration();

    ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type                   Min Max Precision Funct
    entry = {"POWERKEY-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr};
    cfgEntries.push_back(entry);

    setAttachedCANBus(config->canbusNum);

    attachedCANBus->attach(this, deviceID, 0x7F, false); //for canopen devices the ID and mask passed don't actually mean a thing
	
	//we inherited these methods from CanObserver - they allow this class to receive messages automatically routed and interpreted as canopen
	setNodeID(deviceID);
	setCANOpenMode(true);

	setAlive();

	//delay(125);
	//canHandlerBus1.sendNodeStart(deviceID); //tell the keypad to enable itself
	//delay(100);
	//sendAutoStart();
	
	systemIO.installExtendedIO(this);
}

void PowerkeyPad::handleCanFrame(const CAN_message_t &)
{

}

void PowerkeyPad::handlePDOFrame(CAN_message_t &frame)
{	
    crashHandler.addBreadcrumb(ENCODE_BREAD("PWRKY") + 1);
	if (frame.id == (0x180 + deviceID))
	{
		for (int bit = 0; bit < numDigitalInputs; bit++)
		{
			if (frame.buf[bit / 8] & (1 << (bit % 8)))
			{
				if (buttonState[bit] == false) Logger::debug(POWERKEYPRO, "Key %i was pressed");
				actualState[bit] = true;

				if (latchState[bit] != LatchModes::TOGGLING) {					
					buttonState[bit] = true;				
				}
				else if (!toggleState[bit]) //otherwise, if in toggling mode but we haven't toggled yet then toggle.
				{
					toggleState[bit] = true;
					buttonState[bit] = !buttonState[bit];
				}
			}
			else
			{
				actualState[bit] = false;
				if (buttonState[bit] == true) Logger::debug(POWERKEYPRO, "Key %i was released");
				if (latchState[bit] == LatchModes::NO_LATCHING) buttonState[bit] = false;				
				toggleState[bit] = false;
			}
		}
	}
}

void PowerkeyPad::handleSDORequest(SDO_FRAME &frame)
{
}

void PowerkeyPad::handleSDOResponse(SDO_FRAME &frame)
{
}

void PowerkeyPad::handleMessage(uint32_t msgType, void* data)
{
	CANIODevice::handleMessage(msgType, data);
}

DeviceId PowerkeyPad::getId()
{
	return POWERKEYPRO;
}

void PowerkeyPad::setDigitalOutput(int which, bool hi)
{

}

void PowerkeyPad::sendAutoStart()
{
	CAN_message_t autoStartOut;
	autoStartOut.id = 600 + deviceID;
	autoStartOut.len = 8;
	autoStartOut.flags.extended = false;
	autoStartOut.buf[0] = 0x23;
	autoStartOut.buf[1] = 0;
	autoStartOut.buf[2] = 0x65;
	autoStartOut.buf[3] = 1;
	autoStartOut.buf[4] = 0x10;
	autoStartOut.buf[5] = 1;
	autoStartOut.buf[6] = 0;
	autoStartOut.buf[7] = 0;
	attachedCANBus->sendFrame(autoStartOut);
}

/*
used as a stand-in for the LED output. The float value
is broken up into sectors and those are used to trigger
the LEDs. Float values can take a big range so the 
easiest approach is to use the actual integer values
from the other powerkeypro library:
	OFF = 0,
	RED = 1,
	RED_BLINK = 2,
	RED_ALT_BLINK = 3,
	GREEN = 4,
	GREEN_BLINK = 5,
	GREEN_ALT_BLINK = 6,
	AMBER = 7,
	AMBER_BLINK = 8,
	AMBER_ALT_BLINK = 9,
	RED_GREEN_FLASH	= 10,
	AMBER_RED_FLASH = 12,
	GREEN_AMBER_FLASH = 14
*/
void PowerkeyPad::setAnalogOutput(int which, int value)
{
    crashHandler.addBreadcrumb(ENCODE_BREAD("PWRKY") + 2);
    Logger::debug("AnalogOut %i with value %i", which, value);
	if (which == 0 && value == 1000)
	{
		sendLEDBatch();
	}
	else setLEDState(which, (LED::LEDTYPE)value);
}

bool PowerkeyPad::getDigitalInput(int which)
{	
	if (which < 0) return false;
	if (which >= numDigitalInputs) return false;
	bool outputVal = buttonState[which];
	if (latchState[which] == LatchModes::LATCHING) buttonState[which] = actualState[which];
    
    //Logger::debug(getId(), "PKP state of button %i is %i", which, outputVal);
	return outputVal;
}

int16_t PowerkeyPad::getAnalogInput(int which)
{
	return 0; //don't have analog inputs
}

bool PowerkeyPad::getDigitalOutput(int which)
{
	return false; //don't have digital outputs
}

int16_t PowerkeyPad::getAnalogOutput(int which)
{
	return (int16_t)getLEDState(which);
}

void PowerkeyPad::setLatchingMode(int which, LatchModes::LATCHMODE mode)
{
	if (which < 0) return;
	if (which > 11) return;
	latchState[which] = mode;
}

void PowerkeyPad::unlockLatch(int which)
{
	if (which < 0) return;
	if (which > 11) return;
	buttonState[which] = actualState[which];
}

void PowerkeyPad::setLEDState(int which, LED::LEDTYPE state)
{
	if (which < 0) return;
	if (which >= numDigitalInputs) return; //there are as many LEDs as there are buttons

/*	SDO_FRAME frame;
	frame.nodeID = 0x600 + deviceID;
	frame.cmd = SDO_WRITE;
	frame.index = 0x6500;
	frame.subIndex = 1; 
	frame.dataLength = 4;
	frame.data[0] = 1;
	frame.data[1] = which;
	frame.data[2] = (int)state;
	frame.data[3] = 0; 

	canHandlerBus1.sendSDORequest(&frame); */
	LEDState[which] = state;
}

void PowerkeyPad::sendLEDBatch()
{
    crashHandler.addBreadcrumb(ENCODE_BREAD("PWRKY") + 3);
	uint8_t data[8];
	for (int i = 0; i < 8; i++) data[i] = 0;
	for (int i = 0; i < 12; i++)
	{
		if (LEDState[i] & 1)
		{
			if (i < 8) data[0] |= 1 << i;
			else data[1] |= 1 << (i - 8);
		}
		if (LEDState[i] & 4)
		{
			if (i < 4) data[1] |= 16 << i;
			else data[2] |= 1 << (i - 4);
		}
	}
	Logger::debug("LED Batch: %x %x %x", data[0], data[1], data[2]);
	attachedCANBus->sendPDOMessage(0x200 + deviceID, 8, data);
}

LED::LEDTYPE PowerkeyPad::getLEDState(int which)
{
	if (which < 0) return LED::OFF;
	if (which >= numDigitalInputs) return LED::OFF; //there are as many LEDs as there are buttons

	return LEDState[which];
}

void PowerkeyPad::loadConfiguration() {
    PowerKPCANIODeviceConfiguration *config = (PowerKPCANIODeviceConfiguration *)getConfiguration();

    if (!config) {
        config = new PowerKPCANIODeviceConfiguration();
        setConfiguration(config);
    }

    CANIODevice::loadConfiguration(); // call parent

    prefsHandler->read("CanbusNum", &config->canbusNum, 1);
}

void PowerkeyPad::saveConfiguration() {
    PowerKPCANIODeviceConfiguration *config = (PowerKPCANIODeviceConfiguration *)getConfiguration();

    if (!config) {
        config = new PowerKPCANIODeviceConfiguration();
        setConfiguration(config);
    }
    prefsHandler->write("CanbusNum", config->canbusNum);
    CANIODevice::saveConfiguration();
}

DMAMEM PowerkeyPad powerkey;