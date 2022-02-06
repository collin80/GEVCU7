#include "ESP32Driver.h"
#include "gevcu_port.h"
#include "../misc/SystemDevice.h"

/*
Specification for Comm Protocol between ESP32 and GEVCU7 core

First of all, the things that we want to be able to do:
1. Get/Set configuration for the ESP32 - SSID, WPA2Key, Mode
2. Get/Set configuration items for GEVCU7
3. Get performance metrics from running device drivers
4. Get log from sdcard and send it to ESP32
5. Send firmware files from ESP32 to the sdcard
6. Connect to ESP32 from the internet to do remote diag

The first three items can all be done over JSON. Key on { as the first
character to determine that it's JSON and we should process it that way.

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
*/


ESP32Driver::ESP32Driver() : Device()
{
    commonName = "ESP32 Wifi/BT Module";
    shortName = "ESP32";
    currState = ESP32NS::RESET;
    desiredState = ESP32NS::RESET;
    systemAlive = false;
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

    entry = {"ESP32-MODE", "Set ESP32 Mode (0 = Create AP, 1 = Connect to SSID)", &config->esp32_mode, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr};
    cfgEntries.push_back(entry);

    Device::setup(); // run the parent class version of this function

    Serial2.begin(115200);
    Serial2.setTimeout(2);
    Serial2.addMemoryForRead(serialReadBuffer, sizeof(serialReadBuffer));
    Serial2.addMemoryForWrite(serialWriteBuffer, sizeof(serialWriteBuffer));

    pinMode(ESP32_ENABLE, OUTPUT);
    pinMode(ESP32_BOOT, OUTPUT);
    digitalWrite(ESP32_ENABLE, LOW); //start in reset
    digitalWrite(ESP32_BOOT, HIGH); //use normal mode not bootloader mode (bootloader is active low)
    desiredState = ESP32NS::NORMAL;
    //without a large read buffer this tick would have to be fast - like 4ms fast. With a large read buffer
    //the timing can be relaxed. It may be useful to directly catch the serial interrupt callback but then
    //code could be executing at any time. It's all around safer to have deterministic timing via the tick handler
    tickHandler.attach(this, 40000);
}

void ESP32Driver::handleTick() {

    Device::handleTick(); //kick the ball up to papa

    if (currState == ESP32NS::RESET)
    {
        if (desiredState == ESP32NS::NORMAL)
        {
            digitalWrite(ESP32_BOOT, HIGH);
            digitalWrite(ESP32_ENABLE, LOW);
            delay(40);
            digitalWrite(ESP32_ENABLE, HIGH);
            if (sysConfig->systemType == GEVCU7B)  delay(400); //seems we have to wait this long otherwise it won't stick
            else delay(40);
            currState = ESP32NS::NORMAL;
        }
    }

}

//the serial callback is not actually interrupt driven but is called from yield()
//which could get called frequently (and always in the main loop if nothing else)
void ESP32Driver::processSerial()
{
    while (Serial2.available())
    {
        char c = Serial2.read();\
        if (c == '\n')
        {
            Logger::debug("ESP32: %s", bufferedLine.c_str());
            if (bufferedLine.indexOf("BOOTOK") > -1)
            {
                systemAlive = true;
            }

            bufferedLine = "";
        }
        else bufferedLine += c;
    }    
}

//send SSID to ESP32
void ESP32Driver::sendSSID()
{

}

//send password / WPA2 key to esp32
void ESP32Driver::sendPW()
{

}

//whether to be an AP or connect to existing SSID
void ESP32Driver::sendESPMode()
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

    Device::loadConfiguration(); // call parent

}

void ESP32Driver::saveConfiguration() {
    Device::saveConfiguration();
}

ESP32Driver esp32Driver;

void serialEvent2()
{
    esp32Driver.processSerial();
}