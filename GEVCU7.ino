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
now and most stuff should work. There have been two prototypes so far. The first
one wasn't so good but the second is useable with minor hardware fixes.
Both the onboard ESP32 and the MicroMod adapter itself can be updated
via files on the sdcard. Code to log to sdcard is done. It appears that sometimes
the sdcard log file gets corrupted. This might have been due to a previous problem
in the TeensyDuino files themselves. Need to test again to see if sdcard is more
stable now. The board now presents as two serial ports. The first one is the standard
serial console where settings can be changed. The second port is a GVRET compatible
binary protocol port for use with SavvyCAN. This port will forward all traffic on
all three buses to SavvyCAN for debugging and analysis.
 
D0 in documentation is Teensy Pin 4 and is connected to DIG_INT (interrupt from 16 way I/O expander)
D1 in documentation is Teensy Pin 5 and is connected to SD_DET (detect SD card is inserted when line is low)

*/

#include "GEVCU.h"
#include "src/devices/DeviceTypes.h"

// The following includes are required in the .ino file by the Arduino IDE in order to properly
// identify the required libraries for the build.
#include <FlexCAN_T4.h>
#include <TeensyTimerTool.h>
#include "src/i2c_driver_wire.h"
#include <SPI.h>
#include "SdFat.h"
#include "Watchdog_t4.h"
#include "src/devices/esp32/esp_loader.h"
#include "src/devices/esp32/gevcu_port.h"
#include "src/FlasherX.h"
#include "src/devices/misc/SystemDevice.h"

// Use Teensy SDIO - SDIO is four bit and direct in hardware - it should be plenty fast
#define SD_CONFIG  SdioConfig(FIFO_SDIO)

//if this is defined there is a large start up delay so you can see the start up messages. NOT for production!
#define DEBUG_STARTUP_DELAY
//If this is defined then we will ignore the hardware sd inserted signal and just claim it's inserted. Required for first prototype.
//Probably don't enable this on more recent hardware. Starting at the 2nd prototype the sdcard can properly be detected.
//#define ASSUME_SDCARD_INSERTED

//Evil, global variables
MemCache *memCache;
Heartbeat *heartbeat;
SerialConsole *serialConsole;
template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; } //Lets us stream SerialUSB

byte i = 0;

SdFs sdCard;

bool sdCardPresent;

WDT_T4<WDT3> wdt; //use the RTWDT which should be the safest one

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
So far, so good. It seems this functionality is working properly.
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
    deviceManager.sendMessage(DEVICE_ANY, INVALID, MSG_STARTUP, NULL); //allows each device to register its preference handler
    deviceManager.sendMessage(DEVICE_ANY, INVALID, MSG_SETUP, NULL); //then use the preference handler to initialize only enabled devices
}

//called when the watchdog triggers because it was not reset properly. Probably means a hangup has occurred.
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
    canHandlerBus0.sendFrame(output);
    output.id = 0x345;
    canHandlerBus1.sendFrame(output);
    output.id = 0x678;
    canHandlerBus2.sendFrame(output);

    delayMicroseconds(200);

    //now try sending a CAN-FD frame
    //Note, I've found that the underlying driver does not like FD frames if the nominal bitrate is 500k.
    //For some reason you must use 1M as the nominal rate or things do not work when sending. It receives
    //just fine with 500k nominal rate and a faster data rate.
    CANFD_message_t fd_out;
    fd_out.id = 0x1AB;
    fd_out.brs = 1; //do the baud rate switch for data section
    fd_out.edl = 1; //do extended data length too
    fd_out.len = 16;
    for (int l = 0; l < 16; l++) fd_out.buf[l] = l * 3;
    canHandlerBus2.sendFrameFD(fd_out);
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

    delay(1500);
    for (int i = 0; i < 8; i++)
    {
        systemIO.setDigitalOutput(i, false);
    }
    delay(500);

    //below turns esp32 on and off while also forwarding anything it sends to the serial console
    /*
    digitalWrite(45, HIGH);
    uint32_t thisTime = millis();
    while ((millis() - thisTime) < 5000)
    {
        while (Serial2.available())
        {
            Serial.write(Serial2.read());
        }   
    }
    digitalWrite(45, LOW); */
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
    pinMode(ESP32_ENABLE, OUTPUT);
    pinMode(ESP32_BOOT, OUTPUT);

    digitalWrite(ESP32_ENABLE, LOW);
    digitalWrite(ESP32_BOOT, HIGH);

#ifdef DEBUG_STARTUP_DELAY
    for (int c = 0; c < 200; c++) {
        delay(25);  //This delay lets you see startup.  But it breaks DMOC645 really badly.  You have to have comm quickly upon start up
        wdt.feed();
    }
#endif

    Logger::setLoglevel((Logger::LogLevel)0); //force debugging logging on during early start up
       
	digitalWrite(BLINK_LED, LOW);
    Serial.begin(1000000);
    SerialUSB1.begin(1000000);
	Serial.println(CFG_VERSION);
	Serial.print("Build number: ");
	Serial.println(CFG_BUILD_NUM);

#ifndef ASSUME_SDCARD_INSERTED
    if (digitalRead(SD_DETECT) == 0)
    {
#endif
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
#ifndef ASSUME_SDCARD_INSERTED
    }
    else
    {
        Serial.println("No sdCard detected.");
        sdCardPresent = false;
    }
#endif

    if (sdCardPresent)
    {
        //here, directly after trying to find the SDCard is the best place to check the sdcard for firmware files
        //and flash them to the appropriate places if they exist.
        FsFile file;
        if (!file.open("GEVCU7.hex", O_READ)) {
            Logger::debug("No teensy firmware to flash. Skipping.");
        }
        else
        {
            Logger::info("Found teensy firmware. Flashing it");
            setup_flasherx();
            start_upgrade(&file);
            file.close();
        }

        //for ESP32 the first thing to do is to upgrade the bootloader if it's there on the sdcard
        //note: the given hex addresses are hardcoded here and correspond to the default 
        //1.2MB program / 1.5MB SPIFFS partitioning
        //also note, wdt probably can't be active when all these flashing routines are running
        //unless wdt.feed() is added to those routines.
        flashESP32("esp32_bootloader.bin", 0x1000);
        flashESP32("esp32_otadata.bin", 0xE000);
        flashESP32("esp32_partitions.bin", 0x8000);
        flashESP32("esp32_program.bin", 0x10000);
        flashESP32("esp32_website.bin", 0x290000ull);
    }

    wdt.feed();

    tickHandler.setup();

	Wire.begin();
	Logger::info("TWI init ok");
	memCache = new MemCache();
	Logger::info("add MemCache (id: %X, %X)", MEMCACHE, memCache);
	memCache->setup();
    //force the system device to be set enabled. ALWAYS. It would not be good if it weren't enabled!
	PrefHandler::setDeviceStatus(SYSTEM, true);
    //Also, the system device has to be initialized a bit early.
    Device *sysDev = deviceManager.getDeviceByID(SYSTEM);
    sysDev->earlyInit();
    sysDev->setup();
    Logger::console("LogLevel: %i", sysConfig->logLevel);
	//Logger::setLoglevel((Logger::LogLevel)sysConfig->logLevel);
	systemIO.setup();
	canHandlerBus0.setup();
	canHandlerBus1.setup();
    canHandlerBus2.setup();
    //canHandlerBus0.setSWMode(SW_NORMAL); //you can't do this unless you do hardware mods.
	Logger::info("SYSIO init ok");
    deviceManager.setup();

	initializeDevices();
    Logger::debug("Initialized all devices successfully!");
    serialConsole = new SerialConsole(memCache, heartbeat);
	serialConsole->printMenu();
	//btDevice = static_cast<ADAFRUITBLE *>(deviceManager.getDeviceByID(ADABLUE));
    //deviceManager.sendMessage(DEVICE_WIFI, ADABLUE, MSG_CONFIG_CHANGE, NULL); //Load config into BLE interface

	Logger::info("System Ready");

    //just for testing obviously. Don't leave these uncommented.
    sendTestCANFrames();
    //testGEVCUHardware();
    deviceManager.printAllStatusEntries();
}

//there really isn't much in the loop here. Most everything is done via interrupts and timer ticks. If you have
//timer queuing on then those tasks will be dispatched here. Otherwise the loop just cycles very rapidly while
//all the real work is done via interrupt.
void loop() {

//Maybe may want to move the tickhandler process function to a different thread.
//this call could wake up a variety of modules all of which might run code in their tick handlers
//that do a lot of work. So, it's debateable whether that's a good idea to run it all on the main
//thread.
#ifdef CFG_TIMER_USE_QUEUING
	tickHandler.process();
#endif

    //no direct calls here anymore. The serial connections are handled via interrupt callbacks.
	//serialConsole->loop();
    //canHandlerBus0.loop(); //the one loop actually handles incoming traffic for all three
    //canHandlerBus1.loop(); //so no need to call these other two. It's redundant.
    //canHandlerBus2.loop(); //technically you can call them but don't unless some actual need arises?!
    
    //This needs to be called to handle sdCard writing though.
    Logger::loop();
    
    //ESP32 would be our BT device now. Does it need a loop function?
    //if (btDevice) btDevice->loop();

    canEvents();
    
    wdt.feed(); //must feed the watchdog every so often or it'll get angry

    //obviously only for hardware testing. Disable for normal builds.
    //sendTestCANFrames();
    //testGEVCUHardware();
}

//Not interrupt driven but the callbacks should still happen quickly.
//On Teensy MM the serial is interrupt driven and stored into buffers, then 
//if there is data in the buffer to read any call to yield() could call these 
//functions to process data while the original process is waiting for something.
void serialEvent() {
    serialConsole->loop();
}

void serialEventUSB1()
{
    canHandlerBus0.loop();
}
