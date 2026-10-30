#include "ESP32Driver.h"
#include "gevcu_port.h"
#include "../misc/SystemDevice.h"
#include "../../SerialConsole.h"
#include "devices/display/StatusCSV.h"
#include "FlasherX.h"

extern SerialConsole *serialConsole;

/*
Specification for Comm Protocol between ESP32 and GEVCU7 core

First of all, the things that we want to be able to do:
1. Get/Set configuration for the ESP32 - SSID, WPA2Key, Mode (sending to esp32 works now)
2. Get/Set configuration items for GEVCU7 (this would handle being able to set new values)
3. Get performance metrics from running device drivers
4. Get log from sdcard and send it to ESP32
5. Send firmware files from ESP32 to the sdcard
6. Connect to ESP32 from the internet to do remote diag

The first three items can all be done over JSON. Key on { as the first
character to determine that it's JSON and we should process it that way.

For #2 it should be possible for the other side (esp32 running web server) to
query what devices are possible and which are enabled. Enabled devices should
then be able to be queried to get their configuration items. All of this can be
done via json, even the queries. Every message is one line of minified JSON.
Device IDs are always plain JSON numbers (4096, not "0x1000").

Requests from the ESP32 (processed from loop(), never from inside yield()):
{"GetDevices":1}          -> DeviceList reply
{"GetDevConfig":4096}     -> DevConfig reply for that device
{"DeviceID":4096,"CfgName":"SomeSetting","Valu":"12.5"}
                          -> sets a value. Valu is text interpreted exactly as if typed on
                             the serial console (display units, 0x prefix allowed). Replies
                             with CfgResult then a fresh DevConfig for the device.
{"GetStatusList":1}       -> StatusList reply
{"GetStatus":1}           -> re-send every status entry value in the regular status updates

Replies from GEVCU7:
{"DeviceList":{"DMOC645":{"DeviceID":4096,"DeviceName":"DMOC645 Inverter","DeviceEnabled":1,"DeviceType":"MOTORCTRL"}, ...}}
{"DevConfig":{"DMOC645":{"DevID":4096,
    "SomeSetting":{"HelpTxt":"Describe it","Precision":0,"Desc":"optional describe-func text",
                   "Valu":5,"ValType":"INT16","MinValue":-10,"MaxValue":10}, ...}}}
    Valu is the raw stored value. Precision is decimal places for FLOAT. For integers 16 means
    show as hex, negative N means display = stored / |N|, positive N means display = stored * N.
    Min/MaxValue are in display units.
{"CfgResult":{"DeviceID":4096,"CfgName":"SomeSetting","Valu":"12.5","Result":0}}
    Result: 0 = stored, 1 = below minimum, 2 = above maximum, 3 = no such device/setting
{"StatusList":{"DMOC645":["MotorRPM","MotorTorque", ...], ...}}  StatusEntry names grouped by device

Status updates are sent once a second as a flat object of every StatusEntry that changed:
{"MotorRPM":"1234","MotorTorque":"20.5","timeRunning":"1:02:03","systemState":8}

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

//An example of all the parameters that are possible to set right now. SHould probably make it configurable and
//not hard coded.

system state values:
1 Initializing System
2 Precharging System
3 System Pre-Charged
4 Battery Heating
5 Charging
6 Charged
7 Ready
8 Running
9 Shutdown
99 Error
*/

ESP32Driver::ESP32Driver() : Device()
{
    commonName = "ESP32 Wifi/BT Module";
    shortName = "ESP32";
    deviceId = ESP32;
    deviceType = DEVICE_MISC;
    currState = ESP32NS::RESET;
    desiredState = ESP32NS::RESET;
    systemAlive = false;
    systemEnabled = false;
    didInitialStatus = false;
    inhibitJSON = false;
    statusIdx = 0;
    lastTime = 0;
    gotIPAddr = false;
    needWifiConfig = false;
    reqHead = 0;
    reqTail = 0;
    //big enough to hold the initial dump of every status entry in one update
    websocket_json = new DynamicJsonDocument(12000);
}

void ESP32Driver::packageAndSendEntry(StatusEntry *entry)
{
    if (!isReady()) return;
    RecursiveScope m(jsonMutex);
    //ESP32Configuration *config = (ESP32Configuration *) getConfiguration();
     
    (*websocket_json)[entry->statusName] = entry->getValueAsString();
    //serializeJson(doc, Serial2);
    //Serial2.println();

    //shall we send it to the serial console for debugging?
    //if (config->debugMode)
    //{
    //    serializeJsonPretty(doc, Serial);
    //    Serial.println();
    //}
}

void ESP32Driver::handleMessage(uint32_t msgType, const void* message)
{
    Device::handleMessage(msgType, message); //call base class version as it does important stuff
    if (msgType == MSG_CONFIG_CHANGE) //status entry has changed somewhere
    {
        //for now, basically always send out all changes
        StatusEntry *entry = (StatusEntry *)message;//force the cast as we know it will be this type
        packageAndSendEntry(entry);
    }
}

void ESP32Driver::setup()
{
    tickHandler.detach(this);

    Logger::info("add device: ESP32 Module (id:%X, %X)", ESP32, this);

    loadConfiguration();

    ConfigEntry entry;
    entry = {"ESP32-SSID", "Set SSID to create or connect to", &config->ssid, CFG_ENTRY_VAR_TYPE::STRING, 0, 4096, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);

    entry = {"ESP32-PW", "Set WiFi password / WPA2 Key", &config->ssid_pw, CFG_ENTRY_VAR_TYPE::STRING, 0, 4096, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);

    entry = {"ESP32-HOSTNAME", "Set wireless host name (mDNS / OTA)", &config->hostName, CFG_ENTRY_VAR_TYPE::STRING, 0, 4096, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);

    entry = {"ESP32-MODE", "Set ESP32 Mode (0 = Create AP, 1 = Connect to SSID)", &config->esp32_mode, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);

    entry = {"ESP32-DEBUG", "Enable debugging at module level (0 = obey log level, 1 = force debugging on)", &config->debugMode, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);

    entry = {"ESP32-FWUPD", "Update main processor firmware", &config->firmwareFile, CFG_ENTRY_VAR_TYPE::STRING, 0, 4096, 0, nullptr, UPD_PTR(&ESP32Driver::doTeensyUpdate)};
    cfgEntries.push_back(entry);

    entry = {"ESP32-ESPUPD", "Update ESP32 firmware with type", &config->espUpdateType, CFG_ENTRY_VAR_TYPE::BYTE, 0, 9999, 0, nullptr, UPD_PTR(&ESP32Driver::doESP32Update)};
    cfgEntries.push_back(entry);

    entry = {"ESP32-IPADDR", "The current IP address of the ESP32 (DO NOT TRY CHANGING THIS)", &config->ipAddr, CFG_ENTRY_VAR_TYPE::STRING, 0, 40, 0, nullptr, nullptr};
    cfgEntries.push_back(entry);

    Device::setup(); // run the parent class version of this function

    //for some reason nothing works right if you set this higher than 230400 but it doesn't seem like corruption of the characters.
    //Text just... vanishes into thin air. This seems to suggest that one of the ends is going full tilt and overrunning buffers
    Serial2.addMemoryForRead(serialReadBuffer, sizeof(serialReadBuffer));
    Serial2.addMemoryForWrite(serialWriteBuffer, sizeof(serialWriteBuffer));
    Serial2.begin(230400);
    //Serial2.setTimeout(2); //timeout is only used for reading blocks of data. Not for TX

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
    tickHandler.attach(this, 40000, shortName);
    crashHandler.addBreadcrumb(ENCODE_BREAD("ESPTT") + 0);
    deviceManager.addStatusObserver(this); //we want all status updates sent to this object
    lastTime = millis();
}

void ESP32Driver::disableDevice()
{
    Device::disableDevice(); //do the common stuff first
    deviceManager.removeStatusObserver(this);
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
            digitalWrite(ESP32_BOOT, HIGH); //boot high = boot into normal mode
            digitalWrite(ESP32_ENABLE, LOW); //low means reset. High means run. So, we pulse low then high to reset the esp32
            threads.delay(40);
            digitalWrite(ESP32_ENABLE, HIGH);
            if (sysConfig->systemType == GEVCU7B) delay(400); //seems we have to wait this long otherwise it won't stick
            else threads.delay(40);
            currState = ESP32NS::NORMAL;
        }
    }

    if (currState == ESP32NS::NORMAL && systemAlive)
    {
        if (needWifiConfig)
        {
            needWifiConfig = false;
            sendWirelessConfig();
        }

        //packageAndSendEntry drops entries until isReady() so wait for that or the dump is lost
        if (!didInitialStatus && isReady())
        {
            //a chunk of entries per tick gets the whole list into the next once a second update or two
            for (int i = 0; i < 25 && !didInitialStatus; i++)
            {
                StatusEntry entry;
                if (deviceManager.getStatusEntryByIdx(statusIdx++, entry)) packageAndSendEntry(&entry);
                else
                {
                    statusIdx = 0; //start over
                    didInitialStatus = true; //hit the end of the list. Stop sending initial full update
                }
            }
        }
        if (millis() > (lastTime + 1000)) //every second send an update to the web server giving the uptime
        {
            lastTime = millis();

            String buff;
            int totalSeconds = (lastTime / 1000);
            int hours = totalSeconds / 3600;
            int minutes = (totalSeconds / 60) % 60;
            int seconds = totalSeconds % 60;
            buff = "";
            if (hours > 0) buff += String(hours) + ":";
            if (minutes > 0)
            {
                if (minutes < 10) buff += "0";
                buff += String(minutes) + ":";
            }
            if (seconds < 10) buff += "0";
            buff += String(seconds);
            RecursiveScope m(jsonMutex);
            (*websocket_json)["timeRunning"] = buff;
            (*websocket_json)["systemState"] = 8;
            if (!inhibitJSON)
            {
                bool sent = false;
                {
                    RecursiveScope tx(txMutex);
                    if (Serial2.availableForWrite() >= (int)(measureJson((*websocket_json)) + 2))
                    {
                        serializeJson((*websocket_json), Serial2);
                        Serial2.println();
                        sent = true;
                    }
                }
                if (!sent) Logger::warn("ESP32 TX buffer full. Dropping JSON update"); //not while holding txMutex

                //shall we send it to the serial console for debugging?
                if (config->debugMode)
                {
                    serializeJsonPretty((*websocket_json), Serial);
                    Serial.println();
                }
            }
            (*websocket_json).clear();
        }
    }

    crashHandler.updateBreadcrumb(2); //nothing above would add a breadcrumb so update the existing one
}

void ESP32Driver::sendLogString(const char *str)
{
    if (!isReady()) return; //can't do anything until the system is actually up
    RecursiveScope tx(txMutex);
    if (Serial2.availableForWrite() < (int)(strlen(str) + 3)) return; //not enough room for ~, the string, and CRLF. Drop it.
    Serial2.print('~'); //~ prefix means this is a telnet message
    Serial2.println(str);
}

void ESP32Driver::sendStatusCSV(const char *str)
{
    if (!isReady()) return; //can't do anything until the system is actually up
    RecursiveScope tx(txMutex);
    if (Serial2.availableForWrite() < (int)(strlen(str) + 3)) return; //not enough room for `, the string, and CRLF. Drop it.
    Serial2.print('`'); //` prefix means this is a StatusCSV message which should go to the second telnet interface
    Serial2.println(str);
}

bool ESP32Driver::isReady()
{
    if ((currState == ESP32NS::NORMAL) && gotIPAddr && systemAlive) return true;
    return false;
}

//the serial callback is not actually interrupt driven but is called from yield()
//which could get called frequently (and always in the main loop if nothing else)
void ESP32Driver::processSerial()
{
    if (!systemEnabled) return;
    while (Serial2.available())
    {
        char c = Serial2.read();
        //if (config->debugMode) Logger::console("Got char from ESP32: %i", c);
        //if (fileSender->isActive())
        //{
        //    fileSender->processCharacter(c);
        //}
        //else
        {
            if (c == 0xD0) fileSender->processCharacter(c);
            if (c == 0xA5)
            {
                Logger::info("Starting to stream firmware update");
                inhibitJSON = true;
                start_upgrade(nullptr); //start a teensy mm upgrade through serial tunnel
                inhibitJSON = false;
            } 
            else if (c == '\n')
            {
                if (config->debugMode) Logger::console("ESP32: %s", bufferedLine.c_str());
                if (bufferedLine.indexOf("BOOTOK") > -1)
                {
                    systemAlive = true;
                    Logger::info("ESP32 Booted OK");
                    needWifiConfig = true; //sent from handleTick, can't write Serial2 under txMutex from in here
                    //ESP32 (re)booted so it has no status values cached. Send it all of them again.
                    statusIdx = 0;
                    didInitialStatus = false;
                }

                if (bufferedLine.indexOf("IP:") > -1)
                {
                    gotIPAddr = true;
                    Logger::debug("Reported IP Addr: %s", bufferedLine.substring(4).c_str());
                    strncpy(config->ipAddr, bufferedLine.substring(4).c_str(), 39);
                }

                if (bufferedLine[0] == '{')
                {
                    queueRequest(bufferedLine); //handled later by processQueuedRequests from loop()
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
            else if ((c < 128) && bufferedLine.length() < ESP32_REQ_MAX_LEN) bufferedLine += c;
        }
    }    
}

//Write prefix + minified doc + suffix to the ESP32 as one line. The whole line is put into the TX buffer
//in one go under txMutex so nothing else can get spliced into the middle of it. Waits (without holding
//any lock) up to half a second for the buffer to have room. Must not be called from inside yield().
bool ESP32Driver::sendJsonLine(const char *prefix, JsonDocument &doc, const char *suffix)
{
    int len = strlen(prefix) + measureJson(doc) + strlen(suffix) + 2;
    if (len > (int)sizeof(serialWriteBuffer))
    {
        Logger::warn("ESP32 JSON message of %i bytes is too large to send", len);
        return false;
    }

    bool sent = false;
    uint32_t start = millis();
    while (!sent && (millis() - start) < 500)
    {
        {
            RecursiveScope tx(txMutex);
            if (Serial2.availableForWrite() >= len)
            {
                Serial2.print(prefix);
                serializeJson(doc, Serial2);
                Serial2.println(suffix);
                sent = true;
            }
        }
        if (!sent) threads.delay(5);
    }
    if (!sent) Logger::warn("ESP32 TX buffer full. Dropping JSON message");

    //shall we send it to the serial console for debugging?
    if (config->debugMode)
    {
        serializeJsonPretty(doc, Serial);
        Serial.println();
    }
    return sent;
}

//send wireless configuration to ESP32 and cause it to attempt to start up wireless comm
//Note to self, JSON is case sensitive so make sure the letters are in the proper case or it don't work bro.
void ESP32Driver::sendWirelessConfig()
{
    Logger::debug("Sending wifi cfg to ESP32");

    DynamicJsonDocument doc(512); //tick threads have small stacks so keep json docs on the heap
    doc["SSID"] = config->ssid;
    doc["WIFIPW"] = config->ssid_pw;
    doc["WiFiMode"] = config->esp32_mode;
    doc["HostName"] = config->hostName;
    sendJsonLine("", doc, "");
}

void ESP32Driver::sendDeviceList()
{
    DynamicJsonDocument doc(10000);

    deviceManager.createJsonDeviceList(doc);
    sendJsonLine("{\"DeviceList\":", doc, "}");
}

void ESP32Driver::sendDeviceDetails(uint16_t deviceID)
{
    Device *dev = nullptr;
    DynamicJsonDocument doc(12000);

    dev = deviceManager.getDeviceByID(deviceID);
    if (!dev) return;

    deviceManager.createJsonConfigDocForID(doc, deviceID);
    if (doc.overflowed()) Logger::warn("Config for device %X did not fit in the JSON document", deviceID);
    sendJsonLine("{\"DevConfig\":", doc, "}");
}

//names of every StatusEntry grouped by the short name of the device that registered it
void ESP32Driver::sendStatusList()
{
    DynamicJsonDocument doc(12000);
    StatusEntry entry;
    for (unsigned int idx = 0; deviceManager.getStatusEntryByIdx(idx, entry); idx++)
    {
        const char *devName = entry.device ? entry.device->getShortName() : "Other";
        JsonArray arr = doc[devName];
        if (arr.isNull()) arr = doc.createNestedArray(devName);
        arr.add(entry.statusName); //String so json copies it
    }
    if (doc.overflowed()) Logger::warn("Status list did not fit in the JSON document");
    sendJsonLine("{\"StatusList\":", doc, "}");
}

//The ESP32 (web page) wants to set a configuration value.
//{"DeviceID":4096,"CfgName":"SomeSetting","Valu":"12.5"}
void ESP32Driver::processConfigReply(JsonDocument* doc)
{
    uint16_t devID = (*doc)["DeviceID"];
    const char *cfgName = (*doc)["CfgName"] | "";
    String valu = (*doc)["Valu"].as<String>(); //accepts the value whether sent as a string or a number

    int result = 3;
    Device *dev = deviceManager.getDeviceByID(devID);
    const ConfigEntry *entry = nullptr;
    if (dev && dev->isEnabled()) entry = dev->findConfigEntry(cfgName);
    if (entry)
    {
        //applyConfigValue takes a C string. Make a private copy as STRING entries are strcpy'd from it
        char valBuf[ESP32_REQ_MAX_LEN + 1];
        strlcpy(valBuf, valu.c_str(), sizeof(valBuf));
        result = deviceManager.applyConfigValue(dev, entry, valBuf);
        if (result == 0) Logger::info("Web interface set %s to %s", cfgName, valBuf);
    }

    DynamicJsonDocument reply(512);
    reply["DeviceID"] = devID;
    reply["CfgName"] = cfgName;
    reply["Valu"] = valu;
    reply["Result"] = result;
    sendJsonLine("{\"CfgResult\":", reply, "}");

    if (dev) sendDeviceDetails(devID); //so the page shows what is actually stored now
}

//Called from processSerial inside yield() so only copy the line. Drops it if the queue is full.
void ESP32Driver::queueRequest(const String &line)
{
    uint8_t head = reqHead.load();
    uint8_t next = (head + 1) % ESP32_REQ_QUEUE_SIZE;
    if (next == reqTail.load()) return; //full
    strlcpy(reqQueue[head], line.c_str(), sizeof(reqQueue[head]));
    reqHead.store(next); //publish only after the slot is fully written
}

//Runs from loop() on thread 0 where no locks are held and there's plenty of stack. Setting a value saves
//to EEPROM and may run an afterUpdate function so this must not happen inside yield() or a tick thread.
void ESP32Driver::processQueuedRequests()
{
    if (!systemEnabled) return;
    while (reqTail.load() != reqHead.load())
    {
        uint8_t tail = reqTail.load();
        DynamicJsonDocument doc(1024);
        DeserializationError err = deserializeJson(doc, (const char *)reqQueue[tail]);
        reqTail.store((tail + 1) % ESP32_REQ_QUEUE_SIZE); //doc has its own copy now so free the slot
        if (err)
        {
            Logger::error("deserializeJson() failed with code %s", err.f_str());
            continue;
        }

        if (doc["GetDevices"] == 1) sendDeviceList();

        uint16_t devID = doc["GetDevConfig"];
        if (devID > 0) sendDeviceDetails(devID);

        if (doc["GetStatusList"] == 1) sendStatusList();

        if (doc["GetStatus"] == 1)
        {
            statusIdx = 0;
            didInitialStatus = false; //handleTick will send every status entry again
        }

        devID = doc["DeviceID"];
        if (devID > 0) processConfigReply(&doc);
    }
}

uint32_t ESP32Driver::getTickInterval()
{
    return 5000;
}

//send json request to esp32 to start a teensy update from internet
void ESP32Driver::doTeensyUpdate()
{
    Logger::info("Sending microMod OTA request to ESP32");

    inhibitJSON = true;

    DynamicJsonDocument doc(512);
    doc["FWUPD"] = config->firmwareFile;
    sendJsonLine("", doc, "");
}

//send json request to esp32 to start an update on itself
void ESP32Driver::doESP32Update()
{
    Logger::info("Sending esp32 OTA request to ESP32");

    inhibitJSON = true;

    DynamicJsonDocument doc(512);
    doc["ESPUPD"] = config->espUpdateType;
    sendJsonLine("", doc, "");
}

void ESP32Driver::loadConfiguration() {
    
    config = (ESP32Configuration *)getConfiguration();

    if (!config) {
        config = new ESP32Configuration();
        strcpy(config->ipAddr, "NO IP SET");
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

    config->espUpdateType = 1;
    strcpy((char *)config->firmwareFile, "GEVCU7");

    Device::loadConfiguration(); // call parent
}

void ESP32Driver::saveConfiguration() {
    Device::saveConfiguration();

    config = (ESP32Configuration *)getConfiguration();

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