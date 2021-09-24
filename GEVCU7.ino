/*
 GEVCU7.ino
 New hardware version 7/20/2021 for Hardware 7.0
 Author: Collin Kidder
 
Copyright (c) 2013-2021 Collin Kidder, Michael Neuweiler, Charles Galpin

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */
 
/*
Notes on GEVCU7 conversion - Quite far into the conversion process. it compiles
now and most stuff should kind of work. No hardware to test on though.
Currently nothing is being done with the onboard ESP32. It must be able to
be programmed from this sketch. I think the easiest approach is to allow
firmware upgrades from sdcard. Hopefully both processors can be updated
from sdcard. I would also like to be able to log to sdcard to get longer
running logs of the system. Addionally, it would be nice to be able to 
log entire CAN buses to sdcard for analysis. This could be done either in
GVRET format or some special binary format to make it more efficient.

D0 in documentation is Teensy Pin 4 and is connected to DIG_INT (interrupt from 16 way I/O expander)
D1 in documentation is Teensy Pin 5 and is connected to SD_DET (detect SD card is inserted when line is low)

*/

#include "GEVCU.h"
#include "src/devices/DeviceTypes.h"

// The following includes are required in the .ino file by the Arduino IDE in order to properly
// identify the required libraries for the build.
#include <FlexCAN_T4.h>
#include "src/i2c_driver_wire.h"
#include <SPI.h>
#include "SdFat.h"
#include "Watchdog_t4.h"

// Use Teensy SDIO
#define SD_CONFIG  SdioConfig(FIFO_SDIO)

#define DEBUG_STARTUP_DELAY         //if this is defined there is a large start up delay so you can see the start up messages. NOT for production!

//Evil, global variables
PrefHandler *sysPrefs;
MemCache *memCache;
Heartbeat *heartbeat;
SerialConsole *serialConsole;
template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; } //Lets us stream SerialUSB

byte i = 0;

SdFs sdCard;

bool sdCardPresent;

WDT_T4<WDT3> wdt; //use the RTWDT which should be the safest one

//initializes all the system EEPROM values. Chances are this should be broken out a bit but
//there is only one checksum check for all of them so it's simple to do it all here.

void initSysEEPROM() {
	//three temporary storage places to make saving to EEPROM easy
	uint8_t eight;
	uint16_t sixteen;
	uint32_t thirtytwo;

	eight = 7; //GEVCU 7.0 board
	sysPrefs->write(EESYS_SYSTEM_TYPE, eight);

	sixteen = 1024; //no gain
	sysPrefs->write(EESYS_ADC0_GAIN, sixteen);
	sysPrefs->write(EESYS_ADC1_GAIN, sixteen);
	sysPrefs->write(EESYS_ADC2_GAIN, sixteen);
	sysPrefs->write(EESYS_ADC3_GAIN, sixteen);
	sysPrefs->write(EESYS_ADC4_GAIN, sixteen);
	sysPrefs->write(EESYS_ADC5_GAIN, sixteen);
	sysPrefs->write(EESYS_ADC6_GAIN, sixteen);
	sysPrefs->write(EESYS_ADC7_GAIN, sixteen);

	sixteen = 0; //no offset
	sysPrefs->write(EESYS_ADC0_OFFSET, sixteen);
	sysPrefs->write(EESYS_ADC1_OFFSET, sixteen);
	sysPrefs->write(EESYS_ADC2_OFFSET, sixteen);
	sysPrefs->write(EESYS_ADC3_OFFSET, sixteen);
	sysPrefs->write(EESYS_ADC4_OFFSET, sixteen);
	sysPrefs->write(EESYS_ADC5_OFFSET, sixteen);
	sysPrefs->write(EESYS_ADC6_OFFSET, sixteen);
	sysPrefs->write(EESYS_ADC7_OFFSET, sixteen);


	sixteen = CFG_CAN0_SPEED;
	sysPrefs->write(EESYS_CAN0_BAUD, sixteen);
    sixteen = CFG_CAN1_SPEED;
	sysPrefs->write(EESYS_CAN1_BAUD, sixteen);
	sixteen = CFG_CAN2_SPEED;
	sysPrefs->write(EESYS_CAN2_BAUD, sixteen);
    sixteen = 11111; //tripled so 33.333k speed
	sysPrefs->write(EESYS_SWCAN_BAUD, sixteen);


	eight = 2;  //0=debug, 1=info,2=warn,3=error,4=off
	sysPrefs->write(EESYS_LOG_LEVEL, eight);

	sysPrefs->saveChecksum();
    sysPrefs->forceCacheWrite();
}

/*
Here lies where the old "createObjects" function was. There is no need for that.
Now each device driver that should be registered to the system instead just 
creates an object for itself at the end of the CPP file. The creation of that
object registers it with the system automatically. This also allows selective
compilation. If you don't want a specific device driver, just don't compile and
link it. If you want secret device drivers just don't push them into public source
repositories. Now drivers are basically self contained.
NOTE: This means that the preference handler should not be initialized until
the setup() method and that the device manager must be already instantiated
when the devices try to register. Hopefully this happens but if not it might
be necessary to switch the device handler to a singleton and have it automatically
create itself on first access.
*/


void initializeDevices() {
    //heartbeat is always enabled now
    heartbeat = new Heartbeat();
    Logger::info("add: Heartbeat (id: %X, %X)", HEARTBEAT, heartbeat);
    heartbeat->setup();

    //fault handler is always enabled too - its also statically allocated so no using -> here
    faultHandler.setup();

    //send message is not asynchronous so it will block for as long as it takes for all devices to complete.
    //this makes initialization easier but means a device could freeze the system. Might want to do
    //asynchronous or threaded messages at some point but that opens up many other cans of worms.
    deviceManager.sendMessage(DEVICE_ANY, INVALID, MSG_STARTUP, NULL); //allows each device to register it's preference handler
    deviceManager.sendMessage(DEVICE_ANY, INVALID, MSG_SETUP, NULL); //then use the preference handler to initialize only enabled devices

    //sysPrefs->forceCacheWrite(); //if anything updated configuration during init then save that updated info immediately
}

void wdtCallback() 
{
    Serial.println("Watchdog was not fed. It will eat you soon. Sorry...");
}

void sendTestCANFrames()
{
    CAN_message_t output;
    output.len = 8;
    output.id = 0x123;
    output.flags.extended = 0; //standard frame
    output.buf[0] = 2;
    output.buf[1] = 127;
    output.buf[2] = 0;
    output.buf[3] = 52;
    output.buf[4] = 26;
    output.buf[5] = 59;
    output.buf[6] = 4;
    output.buf[7] = 0xAB;
    canHandlerEv.sendFrame(output);
    output.id = 0x345;
	canHandlerCar.sendFrame(output);
    output.id = 0x678;
    canHandlerCar2.sendFrame(output);
    output.id = 0x789;
    canHandlerSingleWire.sendFrame(output);
}

void testGEVCUHardware()
{
    int val;
    Serial.print("ADC: ");
    for (int i = 0; i < 8; i++)
    {
        val = systemIO.getAnalogIn(i);
        Serial.print(val);
        Serial.print("  ");
    }
    Serial.println();

    Serial.print("DIN: ");
    for (int i = 0; i < 12; i++)
    {
        if (systemIO.getDigitalIn(i)) Serial.print("1  ");
        else Serial.print("0  ");
    }
    Serial.println();

    for (int i = 0; i < 8; i++)
    {
        systemIO.setDigitalOutput(i, true);
    }

    delay(500);
    for (int i = 0; i < 8; i++)
    {
        systemIO.setDigitalOutput(i, false);
    }
    delay(500);
    digitalWrite(45, HIGH);
    uint32_t thisTime = millis();
    while ((millis() - thisTime) < 5000)
    {
        while (Serial2.available())
        {
            Serial.write(Serial2.read());
        }   
    }
    digitalWrite(45, LOW);
}

void setup() {
    WDT_timings_t config;
    //GEVCU might loop very rapidly sometimes so windowing mode would be tough. Revisit later
    //config.window = 100; /* in milliseconds, 32ms to 522.232s, must be smaller than timeout */
    config.timeout = 5000; /* in milliseconds, 32ms to 522.232s */
    config.callback = wdtCallback;
    //wdt.begin(config);

    pinMode(BLINK_LED, OUTPUT);
    pinMode(SD_DETECT, INPUT_PULLUP);

#ifdef DEBUG_STARTUP_DELAY
    for (int c = 0; c < 200; c++) {
        delay(25);  //This delay lets you see startup.  But it breaks DMOC645 really badly.  You have to have comm quickly upon start up
        wdt.feed();
    }
#endif

    Logger::setLoglevel((Logger::LogLevel)0); //force debugging logging on during early start up
       
	digitalWrite(BLINK_LED, LOW);
    Serial.begin(115200);
	Serial.println(CFG_VERSION);
	Serial.print("Build number: ");
	Serial.println(CFG_BUILD_NUM);

    if (!digitalRead(SD_DETECT))
    {
        Serial.print("Attempting to mount sdCard ");
	    //init SD card early so we can use it for logging everything else if needed.
	    if (!sdCard.begin(SD_CONFIG))
	    {
    	    //sdCard.initErrorHalt(&Serial);
            Serial.println("- Could not initialize sdCard");
            sdCardPresent = false;
  	    }
        else 
        {
            sdCardPresent = true;
            Serial.println(" OK!");
            Logger::initializeFile();
        }
    }
    else
    {
        Serial.println("No sdCard detected.");
        sdCardPresent = false;
    }

    wdt.feed();

    tickHandler.setup();

	Wire.begin();
	Logger::info("TWI init ok");
	memCache = new MemCache();
	Logger::info("add MemCache (id: %X, %X)", MEMCACHE, memCache);
	memCache->setup();
	sysPrefs = new PrefHandler(SYSTEM);
	if (!sysPrefs->checksumValid()) 
        {
	      Logger::info("Initializing system EEPROM settings");
	      initSysEEPROM();
	    } 
        else {Logger::info("Using existing EEPROM system values");}//checksum is good, read in the values stored in EEPROM

	uint8_t loglevel;
	sysPrefs->read(EESYS_LOG_LEVEL, &loglevel);
	loglevel = 0; //force debugging log level
    Logger::console("LogLevel: %i", loglevel);
	Logger::setLoglevel((Logger::LogLevel)loglevel);
    //Logger::setLoglevel((Logger::LogLevel)0);
	systemIO.setup();  
	canHandlerEv.setup();
	canHandlerCar.setup();
    canHandlerCar2.setup();
    //canHandlerSingleWire.setup();
	Logger::info("SYSIO init ok");	

	initializeDevices();
    serialConsole = new SerialConsole(memCache, heartbeat);
	serialConsole->printMenu();
	//btDevice = static_cast<ADAFRUITBLE *>(deviceManager.getDeviceByID(ADABLUE));
    //deviceManager.sendMessage(DEVICE_WIFI, ADABLUE, MSG_CONFIG_CHANGE, NULL); //Load config into BLE interface

	Logger::info("System Ready");

    sendTestCANFrames();
    testGEVCUHardware();
}

//there really isn't much in the loop here. Most everything is done via interrupts and timer ticks. If you have
//timer queuing on then those tasks will be dispatched here. Otherwise the loop just cycles very rapidly while
//all the real work is done via interrupt.
void loop() {
#ifdef CFG_TIMER_USE_QUEUING
	tickHandler.process();
#endif

	serialConsole->loop();
    Logger::loop();
    
    //if (btDevice) btDevice->loop();
    
    wdt.feed();

    //testGEVCUHardware();
}
