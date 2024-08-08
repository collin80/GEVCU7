#include "ESP32Driver.h"
#include "gevcu_port.h"
#include "../misc/SystemDevice.h"
#include "../../SerialConsole.h"
#include "devices/display/StatusCSV.h"

extern SerialConsole *serialConsole;

/*
Specification for Comm Protocol between ESP32 and GEVCU7 core

First of all, the things that we want to be able to do:
1. Get/Set configuration for the ESP32 - SSID, WPA2Key, Mode (sending to esp32 works now)
2. Get/Set configuration items for GEVCU7 (this would handle being able to set new values)
3. Get performance metrics from running device drivers (faked out right now)
4. Get log from sdcard and send it to ESP32
5. Send firmware files from ESP32 to the sdcard
6. Connect to ESP32 from the internet to do remote diag

The first three items can all be done over JSON. Key on { as the first
character to determine that it's JSON and we should process it that way.

The ESP32 should return the string "BOOTOK" once it is fully loaded
and has booted successfully. This allows the code here to know that the ESP32
is running properly and ready for input.

For #2 it should be possible for the other side (esp32 running web server) to
query what devices are possible and which are enabled. Enabled devices should
then be able to be queried to get their configuration items. All of this can be
done via json, even the queries. 
{"GetDevices":1} is sufficient to ask for device list. 
The returned list (obviously without all the whitespace):
{
    "DeviceList":[
        {
            "DeviceID":"0x1000"
            "DeviceName":"DMOC645 Inverter",
            "DeviceEnabled":0,
            "DeviceType":"Motor Controller"
        },
        {
            "DeviceID":"0x2000"
            "DeviceName":"Potentiometer Accelerator",
            "DeviceEnabled":1,
            "DeviceType":"Throttle"
        }
    ]
}
Then ask for parameters for a specific device:
{"GetDevConfig":"0x1000"}
This returns a list of all parameters with their details:
{
    "DeviceID":"0x1000",
    "DeviceDetails":[
        {
            "CfgName":"SomeSetting",
            "HelpTxt":"Describe what this thing does",
            "Valu":"SomeValue or integer",
            "ValType":"INT16",
            "MinValue":"-10",
            "MaxValue":"10",
            "Precision":4
        }
    ]
}
if the ESP32 side changes the value it must reply back with the change:
{
    "DeviceID":"0x1000",
    "CfgName":"SomeSetting",
    "Valu":"New Value"
}
GEVCU knows the way the value should be interpreted so it can process things
and do the actual setting update.

For #4 there is a special method:
Send 0xB0 followed by the desired log number (0=current, 1-4 are historical)
GEVCU7 returns 0xC0 followed by a 32 bit value for the logsize
Then ESP32 sends 0xC1 followed by the desired size to return. This size
is taken from the end of the file. So, asking for 10k will get the last 10k
of the file. The file is returned in 256 byte chunks, each of which starts
with 0xCA then an 8 bit counter, 256 bytes of log, then CRC8. Bytes past the
end of the file are 0x0 to pad out 256 bytes returned. In this way any chunk
of the log can be gotten. Log files are going to be rotated by GEVCU7 probably
around the 100MB mark so you'd have a current logfile then potentially some
older logs, maybe 3-4 older logs and anything older is deleted.

#5 works similarly:
ESP32 sends 0xD0 to GEVCU7 to signal the start of a firmware upload. Then 
it sends a null terminated string with the desired filename then the
file size as a 32 bit value. Once this is done GEVCU7 sends back 0xD1
if everything is OK. Upon OK the ESP32 sends 256 byte chunks much like
above - Send 0xDA followed by an 8 bit counter, followed by 256 bytes of
firmware, followed by CRC8. Once again, pad the comm to 256 chunks but in
this case GEVCU7 will silently drop any bytes past the end of the file size.

But, the real question is how will the ESP32 get these firmware files? Can't be by
magic! So, probably this has to be either the internet (connect to my server)
or the app has to be able to connect to the internet and do it. The media3.evtv.me
site would be a decent place to put the files.

#6 is trickier. We do NOT want this active unless the owner of the GEVCU7
has specifically requested it but the case is waterproof, sealed, and hard to open.
So, the most logical approach is to allow a digital input to trigger the ability
to do this. GEVCU7 has lots of digital inputs already and I/O expanders can make
that even larger. If that input is triggered we will tell ESP32 to allow remote
control. Send 0xFA 0xCE 0x57 0xA8 to ESP32. This will cause it to connect to
an internet host if it was set to connect to an AP instead of creating one.
If it was set to create an AP then that's problematic. In that case a shim
program that runs on a PC, phone, or tablet would probably be required.
Once it has gotten internet access it should then be possible to do remote
diagnostics, perhaps using the above to ask for logs or update firmware or
even grab json. Anything that's possible through the GEVCU7 to ESP32 link should
be possible remotely too but only when requested. The shim app wouldn't be a big
deal really, just have the device connect to the internet and then forward traffic
back and forth. ESP32 will basically just have a telnet port of sorts. For
added security each ESP32 should have a random key that must be sent to unlock
this comm channel. Once the internet has been contacted any new connection
to the ESP32 should first require the unique code. 32 bits should be enough
as that's still 4 billion possible codes. And, the ESP32 can quit responding
if too many codes are attempted. Perhaps 10 tries is enough then require a power
cycle to be able to try it again.

File transfers need to work reliably between the teensy and esp32 both for
firmware updates and for sdcard downloads. Really, they're the exact same thing
as either is just a tunnel from the sdcard on the teensy to the esp32 either
coming or going. I looked for XMODEM and YMODEM implmentations for Arduino.
They exist but usually are one sided and sometimes directly use ancient C
code that looks terrible. This serial link is extremely short and low latency
and is very unlikely to drop any characters either. So, to some extent a 
transfer protocol is not needed but using one is better than trusting fate.
So, I'm just going to make a very simple transfer protocol that uses CRC16
(the XMODEM version) and is somewhat like X/Y modem. But, a big difference
in implementation is that I don't care about backward compatibility
and there will not be any waiting around for ACKs with delays. The file
transfer code is going to be essentially non-blocking and work via polling
the timer in a loop function and serial interrupts. This way the rest of the program
can keep running.

Use bundled FastCRC library.

File transfers are started from the sender. It sends 0xD0 as a start of transfer
signal. Then it sends a header with the filename and size. The other side ACKs
that it is ready with 0xAA. Then the sender sends 0xDA, followed by a sequence
number (0-255), followed by 512 bytes (padding if needed), followed by the CRC16
of the 512 bytes. The receiver then ACKs (0xAA, followed by sequence #) to show it
has received the chunk, confirmed the CRC, and is ready for another chunk. 0x26 is instead
used for NAK (was going to use 0x55 but that's just a bit shift away from AA which is not ideal)
If NAK then we need to resend that last packet. 0xFA signals the receiver wants to ABORT.

That's it. We know the filesize so no need to signal the end. We know it's the end when we
received enough bytes. This doesn't cover how the receiver might ask for a file to be
sent to it. In the case of a firmware update, the Teensy isn't going to ask. The ESP32
just plain initiates a transfer when nothing else is happening and then transfers the file.
The teensy wasn't expecting it. But for logs the ESP32 has to ask the Teensy to send the log
so some sort of protocol is needed to have it ask for a log file. This could be done pretty
much like above where the ESP32 sends 0xB0 asking for a log number and giving an offset
so it can ask the Teensy not to send the whole file (in case it's large) or maybe an 
offset of 0 means send the whole thing. But, the Teensy should rotate log files somehow,
perhaps rotate every time the system is started so log 0 is always the currently accumulating
log and 1 is the previous, 2 is previous to that, etc. But, renaming log files would be dumb
as if we had 100 logs you'd have to rename 100 then start a new one. Probably instead should
store in EEPROM the current log number and increment it each time the power is cycled. This way
we can save LogFileXXXX.log or something like that. It can have just as many numbers as necessary
and no more. And, store the log index as 32 bit so you can't possibly power cycle the unit
enough times to overflow it. Still, this may eventually wear out the card and/or fill it up.
Maybe have a limit to the number of logs that can be kept and automatically attempt to delete
the log X back. For instance, if we create LogFile200.log and we're keeping 50 logs we might
try to delete LogFile151.log upon power up. That way it is constantly cleaning up old files
and trying to keep the # of logs managable.

Header could be something like:
struct {
    char filename[128];
    uint32_t filesize;
};
*/


ESP32Driver::ESP32Driver() : Device()
{
    commonName = "ESP32 Wifi/BT Module";
    shortName = "ESP32";
    currState = ESP32NS::RESET;
    desiredState = ESP32NS::RESET;
    systemAlive = false;
    systemEnabled = false;
}

void ESP32Driver::earlyInit()
{
    prefsHandler = new PrefHandler(ESP32);
}

void ESP32Driver::setup()
{
    tickHandler.detach(this);

    Logger::info("add device: ESP32 Module (id:%X, %X)", ESP32, this);

    loadConfiguration();

    ESP32Configuration *config = (ESP32Configuration *) getConfiguration();

    ConfigEntry entry;
    entry = {"ESP32-SSID", "Set SSID to create or connect to", &config->ssid, CFG_ENTRY_VAR_TYPE::STRING, 0, 4096, 0, nullptr};
    cfgEntries.push_back(entry);

    entry = {"ESP32-PW", "Set WiFi password / WPA2 Key", &config->ssid_pw, CFG_ENTRY_VAR_TYPE::STRING, 0, 4096, 0, nullptr};
    cfgEntries.push_back(entry);

    entry = {"ESP32-HOSTNAME", "Set wireless host name (mDNS / OTA)", &config->hostName, CFG_ENTRY_VAR_TYPE::STRING, 0, 4096, 0, nullptr};
    cfgEntries.push_back(entry);

    entry = {"ESP32-MODE", "Set ESP32 Mode (0 = Create AP, 1 = Connect to SSID)", &config->esp32_mode, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr};
    cfgEntries.push_back(entry);

    entry = {"ESP32-DEBUG", "Enable debugging at module level (0 = obey log level, 1 = force debugging on)", &config->debugMode, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr};
    cfgEntries.push_back(entry);

    Device::setup(); // run the parent class version of this function

    //for some reason nothing works right if you set this higher than 115200 but it doesn't seem like corruption of the characters.
    //Text just... vanishes into thin air. This seems to suggest that one of the ends is going full tilt and overrunning buffers
    //Serial2.addMemoryForRead(serialReadBuffer, sizeof(serialReadBuffer));
    //Serial2.addMemoryForWrite(serialWriteBuffer, sizeof(serialWriteBuffer));
    Serial2.begin(230400);
    //Serial2.setTimeout(2);

    fileSender = new SerialFileSender(&Serial2);

    systemEnabled = true;

    pinMode(ESP32_ENABLE, OUTPUT);
    pinMode(ESP32_BOOT, OUTPUT);
    digitalWrite(ESP32_ENABLE, LOW); //start in reset
    digitalWrite(ESP32_BOOT, HIGH); //use normal mode not bootloader mode (bootloader is active low)
    desiredState = ESP32NS::NORMAL;
    //without a large read buffer this tick would have to be fast - like 4ms fast. With a large read buffer
    //the timing can be relaxed. It may be useful to directly catch the serial interrupt callback but then
    //code could be executing at any time. It's all around safer to have deterministic timing via the tick handler
    tickHandler.attach(this, 40000);
    crashHandler.addBreadcrumb(ENCODE_BREAD("ESPTT") + 0);
}

void ESP32Driver::disableDevice()
{
    Device::disableDevice(); //do the common stuff first
    digitalWrite(ESP32_ENABLE, LOW); //put the esp32 into reset
    digitalWrite(ESP32_BOOT, HIGH); //use normal mode
    Serial2.end();
    delete fileSender;
    fileSender = nullptr;
}

void ESP32Driver::handleTick() {

    Device::handleTick(); //kick the ball up to papa
    crashHandler.addBreadcrumb(ENCODE_BREAD("ESPTT") + 1);

    if (currState == ESP32NS::RESET)
    {
        if (desiredState == ESP32NS::NORMAL)
        {
            //TODO: this is naughty code! No delays allowed! Refactor this to remove the delays (use state machine?)
            digitalWrite(ESP32_BOOT, HIGH);
            digitalWrite(ESP32_ENABLE, LOW);
            delay(40);
            digitalWrite(ESP32_ENABLE, HIGH);
            if (sysConfig->systemType == GEVCU7B) delay(400); //seems we have to wait this long otherwise it won't stick
            else delay(40);
            currState = ESP32NS::NORMAL;
        }
    }
    crashHandler.updateBreadcrumb(2); //nothing above would add a breadcrumb so update the existing one
}

void ESP32Driver::sendLogString(String str)
{
    if (!systemAlive) return; //can't do anything until the system is actually up
    Serial2.println("~" + str); //~ prefix means this is a telnet message
}

void ESP32Driver::sendStatusCSV(String str)
{
    if (!systemAlive) return; //can't do anything until the system is actually up
    Serial2.println("`" + str); //` prefix means this is a StatusCSV message which should go to the second telnet interface
}

//the serial callback is not actually interrupt driven but is called from yield()
//which could get called frequently (and always in the main loop if nothing else)
void ESP32Driver::processSerial()
{
    ESP32Configuration *config = (ESP32Configuration *) getConfiguration();
    if (!systemEnabled) return;
    while (Serial2.available())
    {
        char c = Serial2.read();
        //if (config->debugMode) Logger::console("Got char from ESP32: %i", c);
        if (fileSender->isActive())
        {
            fileSender->processCharacter(c);
        }
        else
        {
            if (c == 0xD0) fileSender->processCharacter(c);
            else if (c == '\n')
            {
                if (config->debugMode) Logger::console("ESP32: %s", bufferedLine.c_str());
                if (bufferedLine.indexOf("BOOTOK") > -1)
                {
                    systemAlive = true;
                    Logger::info("ESP32 Booted OK");
                    sendWirelessConfig();
                }

                if (bufferedLine[0] == '{')
                {
                    StaticJsonDocument<1300>doc;
                    DeserializationError err = deserializeJson(doc, bufferedLine.c_str());
                    if (err)
                    {
                        Logger::error("deserializeJson() failed with code %s", err.f_str());
                    }
                    else
                    {
                        if (doc["GetDevices"] == 1)
                        {
                            sendDeviceList();
                        }

                        uint16_t devID = doc["GetDevConfig"];
                        if (devID > 0)
                        {
                            sendDeviceDetails(devID);
                        }

                        devID = doc["DeviceID"];
                        if (devID > 0)
                        {
                            processConfigReply(&doc);
                        }
                    }
                }
                
                if (bufferedLine[0] == '~')
                {
                    //send the whole thing (minus the ~) as input to the normal serial console
                    for (unsigned int l = 1; l < bufferedLine.length(); l++)
                    {
                        serialConsole->injectChar(bufferedLine[l]);
                    }
                    serialConsole->injectChar('\n');
                }
                if (bufferedLine[0] == '`')
                {
                    if (bufferedLine[1] == 's' || bufferedLine[1] == 'S')
                    {
                        StatusCSV *csv = static_cast<StatusCSV *>(deviceManager.getDeviceByID(0x4500));
                        if (csv) csv->toggleOutput();
                    }
                }

                bufferedLine = "";
            }
            else if (c < 128) bufferedLine += c;
        }
    }    
}

//send wireless configuration to ESP32 and cause it to attempt to start up wireless comm
//Note to self, JSON is case sensitive so make sure the letters are in the proper case or it don't work bro.
void ESP32Driver::sendWirelessConfig()
{
    Logger::debug("Sending wifi cfg to ESP32");
    ESP32Configuration *config = (ESP32Configuration *) getConfiguration();
    StaticJsonDocument<300> doc;
    doc["SSID"] = config->ssid;
    doc["WIFIPW"] = config->ssid_pw;
    doc["WiFiMode"] = config->esp32_mode;
    doc["HostName"] = config->hostName;
    serializeJson(doc, Serial2);
    Serial2.println();

    //shall we send it to the serial console for debugging?
    if (config->debugMode)
    {
        serializeJsonPretty(doc, Serial);
        Serial.println();
    }
}

void ESP32Driver::sendDeviceList()
{
    ESP32Configuration *config = (ESP32Configuration *)getConfiguration();
    DynamicJsonDocument doc(10000);

    deviceManager.createJsonDeviceList(doc);

    serializeJson(doc, Serial2);
    Serial2.println();

    //shall we send it to the serial console for debugging?
    if (config->debugMode)
    {
        serializeJsonPretty(doc, Serial);
        Serial.println();
    }
}

void ESP32Driver::sendDeviceDetails(uint16_t deviceID)
{
    ESP32Configuration *config = (ESP32Configuration *)getConfiguration();
    Device *dev = nullptr;
    DynamicJsonDocument doc(10000);

    dev = deviceManager.getDeviceByID(deviceID);
    if (!dev) return;

    deviceManager.createJsonConfigDocForID(doc, deviceID);

    //send minified json to ESP32
    serializeJson(doc, Serial2);
    Serial2.println();

    //shall we send it to the serial console for debugging?
    if (config->debugMode)
    {
        serializeJsonPretty(doc, Serial);
        Serial.println();
    }
}

void ESP32Driver::processConfigReply(JsonDocument* doc)
{

}


DeviceId ESP32Driver::getId() {
    return (ESP32);
}

uint32_t ESP32Driver::getTickInterval()
{
    return 5000;
}

void ESP32Driver::loadConfiguration() {
    
    ESP32Configuration *config = (ESP32Configuration *)getConfiguration();

    if (!config) {
        config = new ESP32Configuration();
        setConfiguration(config);
    }

    prefsHandler->read("SSID", (char *)config->ssid, "GEVCU7");
    prefsHandler->read("WIFIPW", (char *)config->ssid_pw, "Default123");
    prefsHandler->read("HostName", (char *)config->hostName, "gevcu7");
    prefsHandler->read("WiFiMode", &config->esp32_mode, 0); //create an AP
    prefsHandler->read("DebugMode", &config->debugMode, 0);

    Logger::debug("SSID: %s", config->ssid);
    Logger::debug("PW: %s", config->ssid_pw);
    Logger::debug("Hostname: %s", config->hostName);

    Device::loadConfiguration(); // call parent
}

void ESP32Driver::saveConfiguration() {
    Device::saveConfiguration();

    ESP32Configuration *config = (ESP32Configuration *)getConfiguration();

    prefsHandler->write("SSID", (const char *)config->ssid, 64);
    prefsHandler->write("WIFIPW", (const char *)config->ssid_pw, 64);
    prefsHandler->write("HostName", (const char *)config->hostName, 64);
    prefsHandler->write("WiFiMode", config->esp32_mode);
    prefsHandler->write("DebugMode", config->debugMode);
    //prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}

DMAMEM ESP32Driver esp32Driver;

void serialEvent2()
{
    esp32Driver.processSerial();
}