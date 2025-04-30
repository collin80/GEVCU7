/*
 GEVCU7.ino
 New hardware version 7/20/2021 for Hardware 7.0
 Author: Collin Kidder
 
Copyright (c) 2013-2025 Collin Kidder, Michael Neuweiler, Charles Galpin

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
Notes on GEVCU7 conversion. This is somewhat a stream of consciousness but covers
the state of the code and where I plan to take it.


This revision does not have any required board level bodges to get it working. 
The one remaining thing is that some voltage bleeds through the 5V regulator and 
turns on the 12v light but a little bit dimly. This is still happening in
production hardware! Whoops! The fix is to add a diode so that 
voltage cannot backfeed through the regulator.
 
D0 in documentation is Teensy Pin 4 and is connected to DIG_INT (interrupt from 16 way I/O expander)
D1 in documentation is Teensy Pin 5 and is connected to SD_DET (detect SD card is inserted when line is low)

Maybe consider switching to an RTOS but it seems like quite a large change and would probably break
things all over the place. This is not an idea to take lightly. May never happen.

There should be a way for the system to signal it wants to shut down and either stay down or reboot. In those cases,
it should be possible for devices to render things safe. For instance, a motor controller driver would probably
want to set the gear to neutral or park (as applicable. Obviously don't go into park when going 80MPH), a BMS
might want to open contactors safely, etc. But, there is some level of priority here. First off, the inverter
should shut down and quit trying to draw or source current. Also, DC/DC should stop working, then when as much
current as possible is stopped we can open contactors. Then settings can be saved to EEPROM. But, this also brings
up the question of shutdown type. Turning the key off is much different from crashing into a propane truck.
This can also pave the way for allowing GEVCU7 to be on constantly and control the rest of the system intelligently.
If it's always on then it could do safe shutdown then sleep and wake up when the key is turned back to on.
But, what sort of sleep current could the board get to? It can turn off the ESP32 and the digital I/O chip doesn't
really take much power. The CAN tranceivers will not turn off. Testing seems to suggest it can idle around 40ma
and running it is 100ma or more. If a normal car battery is about 40ah then a 40ma draw will drain the battery
dead in 1000 hours or about 41 days. It's probably not ideal to leave GEVCU7 running but still some support should
exist because it overlaps with the need to safety shut down for other reasons.

To support the above, it would be necessary to wire GEVCU7 to a dedicated power wire that always has power and then
designate a different digital input pin to be the key on signal. When key is on we turn on everything we can and enable
the system. When the key is turned off we ask everyone to stop what they're doing, save EEPROM, and go into the lowest power
mode possible. But, this would probably require that GEVCU could also control a contactor or something to actually cut 12V power
to the rest of the car. Then you're hoping that GEVCU7 is stable enough to really be controlling the power for the whole car.
Otherwise nothing works at all. Though, if GEVCU7 dies nothing is really going to work that great in any event. If it is
controlling the inverter then you aren't going unless it's working. So, this might be viable. 

*/

#include "GEVCU.h"
#include "devices/DeviceTypes.h"

// The following includes are required in the .ino file by the Arduino IDE in order to properly
// identify the required libraries for the build.
#include <FlexCAN_T4.h>
#include <TeensyTimerTool.h>
#include "i2c_driver_wire.h"
#include <SPI.h>
#include "SD.h"
#include "Watchdog_t4.h"
#include "devices/esp32/esp_loader.h"
#include "devices/esp32/gevcu_port.h"
#include "FlasherX.h"
#include "devices/misc/SystemDevice.h"
#include "CrashHandler.h"
#include "localconfig.h"

// Use Teensy SDIO - SDIO is four bit and direct in hardware - it should be plenty fast
#define SD_CONFIG  SdioConfig(FIFO_SDIO)

//Evil, global variables
MemCache *memCache;
Heartbeat *heartbeat;
SerialConsole *serialConsole;
template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; } //Lets us stream SerialUSB

byte i = 0;

uint8_t sdCardPresence; //0 = not inserted, positive numbers indicate it has been inserted for increasing time (up to a limit)
bool sdCardWorking; //does it appear that the sdcard is currently working properly and able to be used?
bool sdCardInitFailed; //got a presence signal but trying to init it failed. Quit trying until next insertion
uint32_t bootTime;

WDT_T4<WDT3> wdt; //use the RTWDT which should be the safest one

//if not creating intentional start up delays then squash the default start up delay
#ifndef DEBUG_STARTUP_DELAY
extern "C" void startup_middle_hook(void);
extern "C" volatile uint32_t systick_millis_count;
FLASHMEM void startup_middle_hook(void) {
    // OPTIONAL FASTER STARTUP: force millis() to be 300 to skip startup delays
    systick_millis_count = 300;
}
#endif

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
when the devices try to register. 
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
/* testing code to test the fault handler system.
    int faults = faultHandler.getFaultCount();
    if (faults > 0)
    {
        Logger::info("%i faults found in eeprom", faults);
        while (faults--)
        {
            FAULT *fault;
            if (!(fault = faultHandler.getNextFault())) break;
            Logger::info("Fault address: %x", fault);
            Logger::info("Time Stamp: %i Device: %x Fault Code: %x", fault->timeStamp, fault->device, fault->faultCode);
        }
    }
    else
    {
        Logger::info("Good news, no stored faults!");
    }
*/
}

//called when the watchdog triggers because it was not reset properly. Probably means a hangup has occurred.
void wdtCallback() 
{
    Serial.println("Watchdog was not fed. It will eat you soon. Sorry...");
}

FLASHMEM void sendTestCANFrames()
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

/*
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
*/
}

FLASHMEM void testGEVCUHardware()
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

FLASHMEM void setup() {
    WDT_timings_t config;
    //GEVCU might loop very rapidly sometimes so windowing mode would be tough. Revisit later
    //config.window = 100; /* in milliseconds, 32ms to 522.232s, must be smaller than timeout */
    config.timeout = 5000.0; /* in milliseconds, 32ms to 522.232s */
    config.callback = wdtCallback;

    pinMode(BLINK_LED, OUTPUT);
    pinMode(SD_DETECT, INPUT_PULLUP);
    pinMode(ESP32_ENABLE, OUTPUT);
    pinMode(ESP32_BOOT, OUTPUT);

    digitalWrite(ESP32_ENABLE, LOW); //ESP32 starts turned off
    digitalWrite(ESP32_BOOT, HIGH);

    Logger::setLoglevel((Logger::LogLevel)0); //force debugging logging on during early start up
       
	digitalWrite(BLINK_LED, LOW);
    //Serial.begin will cause the stack to wait until things are initialized (or 2 seconds)
    //whichever is shorter. This basically gives you a 2 second delay upon start up but means
    //you will see the early program output.
#ifdef DEBUG_STARTUP_DELAY
    uint32_t timeBeforeSerial = millis();
    Serial.begin(1000000);
    SerialUSB1.begin(1000000);
    uint32_t timeAfterSerial = millis();
#endif
    
    deviceManager.sortDeviceTable();

    //pretty early in boot we want to know if the previous try crashed
    crashHandler.captureCrashDataOnStartup();

    //breadcrumb system will give you a sort of stack trace if things crash
    crashHandler.addBreadcrumb(ENCODE_BREAD("START"));

    if (Serial)
    {
        Serial.print("Build number: ");
        Serial.println(CFG_BUILD_NUM);
        Serial.print("Build Date: ");
        Serial.print(__DATE__);
        Serial.print(" at ");
        Serial.println(__TIME__);
    }
    sdCardInitFailed = false;

#ifndef ASSUME_SDCARD_INSERTED
    for (int i = 0; i < 4; i++)
    {
        if (!digitalRead(SD_DETECT)) sdCardPresence++;
        else sdCardPresence = 0;
        delay(10); 
    }
    if (sdCardPresence > 1)
    {
#endif
        if (Serial) Serial.print("Attempting to mount sdCard ");
	    //init SD card early so we can use it for logging everything else if needed.
        if (!SD.sdfs.begin(SdioConfig(FIFO_SDIO)))
	    {
    	    //sdCard.initErrorHalt(&Serial);
            if (Serial) Serial.println("- Could not initialize sdCard");
            sdCardWorking = false;
            sdCardInitFailed = true;
  	    }
        else 
        {
            sdCardWorking = true;
            if (Serial) Serial.println(" OK!");
            Logger::initializeFile();
            //if the system crashed we ought to decode all the breadcrumbs and save them into the logfile.
            //Maybe within the above function?
        }
#ifndef ASSUME_SDCARD_INSERTED
    }
    else
    {
        if (Serial) Serial.println("No sdCard detected.");
        sdCardWorking = false;
    }
#endif

    crashHandler.analyzeCrashData();

    if (sdCardWorking)
    {
        //here, directly after trying to find the SDCard is the best place to check the sdcard for firmware files
        //and flash them to the appropriate places if they exist.
        FsFile file;
        file = SD.sdfs.open("GEVCU7.hex", O_READ);
        if (!file) {
            Logger::info("No teensy firmware to flash. Skipping.");
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

    tickHandler.setup();

    Logger::flushFile();

	Wire.begin();
	Logger::info("TWI init ok");
	memCache = new MemCache();
	Logger::info("add MemCache (id: %X, %X)", MEMCACHE, memCache);
	memCache->setup();

    //need to turn this on somewhere. Moved it down pretty low in the power on setup so that things like 
    //firmware updates don't require special handling with the watchdog (at least not power on fw updates)
    //but, it's before any of the non-system device drivers load in case one of them has a bug.
    wdt.begin(config);

    //force the system device to be set enabled. ALWAYS. It would not be good if it weren't enabled!
    Device *sysDev = deviceManager.getDeviceByID(SYSTEM);
    Device *sysIODev = deviceManager.getDeviceByID(SYSIO);
    sysDev->earlyInit();
    sysIODev->earlyInit();
    //all the 0x7?00 devices should always be enabled.
	PrefHandler::setDeviceStatus(SYSTEM, true);
    PrefHandler::setDeviceStatus(HEARTBEAT, true);
    PrefHandler::setDeviceStatus(MEMCACHE, true);
    PrefHandler::setDeviceStatus(SYSIO, true);
    //Also, the system device has to be initialized a bit early.    
    sysDev->setup();

    //log level is set by system device driver just above ^^^^^
    Logger::console("LogLevel: %i", sysConfig->logLevel);
    Logger::flushFile();

    //initialize all the hardware I/O (CAN, digital, analog, etc)
	systemIO.setup();
	canHandlerBus0.setup();
	canHandlerBus1.setup();
    canHandlerBus2.setup();
    //canHandlerBus0.setSWMode(SW_NORMAL); //you can't do this unless you do hardware mods.
	Logger::info("SYSIO init ok");
    deviceManager.setup();

    //if we crashed then do not initialize any of the devices this time around.
    //This may not be the ideal solution. Actually, lots of things were initialized above
    //anyway so only things derived from Device wouldn't get initialized here.
    //if (!crashHandler.bCrashed())
    //{
	    initializeDevices();
    //    Logger::debug("Initialized all devices successfully!");
    //}
    //else
   // {
    //    Logger::warn("Enabled devices were not loaded because last boot crashed.");
   // }

    serialConsole = new SerialConsole(memCache, heartbeat);
    serialConsole->setup();
	serialConsole->printMenu();

	Logger::info("System Ready");
    bootTime = millis();
#ifdef DEBUG_STARTUP_DELAY
    Logger::info("Start up delay was %ims", timeAfterSerial - timeBeforeSerial);
#endif
    //for testing start up speed. Pin 40 is connected to digital input 8 which is easy to grab at the top of a chip
    //pinMode(40, OUTPUT);
    //digitalWrite(40, LOW);
    crashHandler.addBreadcrumb(ENCODE_BREAD("BOOTD"));

    //just for testing. Don't uncomment for production roms
    //systemIO.setDigitalOutputPWM(0, 30, 400);

    //just for testing obviously. Don't leave these uncommented.
    //sendTestCANFrames();
    //testGEVCUHardware();
    //deviceManager.printAllStatusEntries();

    // causes Data_Access_Violation if you uncomment it. Only for testing crash handler
    //I know you're tempted to try it. Don't, things will crash instantly at this next line.
    //    *(volatile uint32_t *)0x30000000 = 0; 

    Logger::flushFile();
}

//there really isn't much in the loop here. Most everything is done via interrupts and timer ticks. If you have
//timer queuing on then those tasks will be dispatched here. Otherwise the loop just cycles very rapidly while
//all the real work is done via interrupt.
void loop() {
#ifdef CFG_TIMER_USE_QUEUING
	tickHandler.process();
#endif
    
    //This needs to be called to handle sdCard writing though.
    Logger::loop();
    
    //ESP32 would be our BT device now. Does it need a loop function?
    //if (btDevice) btDevice->loop();

    canEvents(); //get messages on all three CAN buses and dispatch them

    //Reads SerialUSB1 (if statusCSV isn't active) 
    canHandlerBus0.loop();
    
    wdt.feed(); //must feed the watchdog every so often or it'll get angry

    //obviously only for hardware testing. Disable for normal builds.
    /*
    static uint32_t lastSentTest=0;
    if (millis() - 1000 > lastSentTest)
    {
        sendTestCANFrames();
        lastSentTest = millis();
    }
    */
    //testGEVCUHardware();

    //it should go without saying that uncommenting the below line will give you a bad time... in 40 seconds
    //if (millis() > 40000)     *(volatile uint32_t *)0x30000000 = 0; // causes Data_Access_Violation if you uncomment it. Only for testing crash handler
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
}

//this function blocks out a lot of code that would otherwise be generated to handle exceptions.
//The exception handling code seriously bloats the binary. Maybe change what this function does
//to be something better than blocking infinitely though.
namespace __gnu_cxx
{
    void __verbose_terminate_handler()
    {
        while (1) asm ("WFI");
    }
}