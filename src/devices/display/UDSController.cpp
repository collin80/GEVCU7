/*
 * UDSController.cpp
 *
 * Listens for PID requests over canbus and responds with the relevant information in the proper format
 * Also implements other UDS functions like firmware updates
 *
 * Currently this is in a very rough state and shouldn't be trusted - Make configuration work soon
 *
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

#include "UDSController.h"
#include <Entropy.h>
#include "../../Logger.h"

UDSController udsctrl; //declared up here because it is actually used in this code.

isotp<RX_BANKS_2, 512> udsIsoTPTargetted;
isotp<RX_BANKS_2, 512> udsIsoTPBroadcast;

extern FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> Can0;
extern FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> Can1;
extern FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_16> Can2;

/*
Basic firmware updating idea - use UDS commands but as simply as possible. First off, the other side
must ask for security access level 3 and pass the chal/response. The challenge is 32 bits long
and generated randomly. The response should be taking each challenge byte, multiplying by the 
corresponding byte in the 4 byte magic value then xor with the other table. Then return the new 32 bit value
If that passes then the other side will request a data download to start. This gives us the write position
and the length. From there we can accept chunks which we'll write to flash above where our program is really stored.
After the whole thing is buffered in upper flash we get the signal that firmware sending is done. At that point
we stop everything and copy the firmware from the buffer to real flash storage then immediately reboot.Since the code
that does this is RAM resident this should work without crashing anything. 


*/

//bouncy bounce. Call the member function and that's all.
void udsCallback(const ISOTP_data &iso_config, const uint8_t *buf)
{
    udsctrl.handleIsoTP(iso_config, buf);
}

void UDSController::handleIsoTP(const ISOTP_data &iso_config, const uint8_t *buf)
{
    UDSConfiguration* config = (UDSConfiguration *)getConfiguration();
    ISOTP_data reply;
    uint32_t firmwareSize;
    uint32_t firmwareAddr;

    Logger::debug("UDS SID: %X config: %X", buf[0], config);

    switch (buf[0]) //first data byte is the UDS/OBDII function code
    {
    case OBDII_SHOW_CURRENT: //show current data
        if (processShowData(buf)) //if it returns true then we're handling this request and have a reply
        {
            sendBuffer[0] = buf[0] + 0x40; //0x40 signifies a reply instead of a request
            sendBuffer[1] = buf[1]; //which PID are we replying to?
            reply.id = config->udsTx;
            reply.flags.extended = config->useExtended;
            reply.flags.usePadding = 1;
            reply.separation_time = 1;
            udsIsoTPTargetted.write(reply, sendBuffer, sendBuffer[511] + 2);
        }
        break;
    case OBDII_SHOW_STORED_DTC: //should support this some day.
        break;
    case OBDII_CLEAR_DTC:
        break;
    case OBDII_VEH_INFO: //can return ECU name and perhaps VIN here.
        break;
    case UDS_READ_BY_ID: //potentially support custom PID codes here.
        break;
    case UDS_SECURITY_ACCESS: //chal/resp security access to allow for firmware updates and such
        Logger::debug("UDS Security Access");
        sendBuffer[0] = buf[0] + 0x40; //0x40 signifies a reply instead of a request
        sendBuffer[1] = buf[1]; //which security level are we replying to?        
        reply.id = config->udsTx;
        reply.flags.extended = config->useExtended;
        reply.flags.usePadding = 1;
        reply.separation_time = 1;

        if (buf[1] == 3) //requesting challenge seed
        {
            if (!inSecurityMode)
            {                
                generateChallenge();
                uint32_t challVal;
                memcpy(&challVal, challenge, 4);
                Logger::debug("Challenge Bytes: %X", challVal);
                for (int i = 0; i < 4; i++) sendBuffer[i + 2] = challenge[i];
            }
            else
            {
                for (int i = 0; i < 4; i++) sendBuffer[i + 2] = 0; //all 0's means we're already unlocked
            }
            udsIsoTPTargetted.write(reply, sendBuffer, 6);
        }
        else if (buf[1] == 4) //trying to unlock with response
        {
            if (!generatedSeed) break; //do no validation if the seed has not been freshly generated
            if (validateResponse(&buf[2])) //enter security mode and confirm this with our reply
            {
                inSecurityMode = true;
                udsIsoTPTargetted.write(reply, sendBuffer, 2); //just 0x67 and security level means A-OK
            }
            else //return "nice try, so sad"
            {
                sendBuffer[0] = 0x7F; //the byte of doooooom
                sendBuffer[1] = UDS_SECURITY_ACCESS; //The negative reply corresponds to this SID
                sendBuffer[2] = 0x35; //invalid key!
                udsIsoTPTargetted.write(reply, sendBuffer, 3);
                generatedSeed = false; //can't try again on this seed!
            }
        }                
        break;
    case UDS_REQUEST_DOWNLOAD: //other side wants to send us new firmware
            /*
            The request for download has the following payload:
            first byte: upper nibble is compression type (only support 0), lower nibble is encryption type (only support 0)
            Second byte: upper nibble has the # of bytes used for data length size, lower nibble has address size.
            We'll expect 4 bytes for both. Though, I see no reason to actually listen to the passed address.
            Our address is static and the code already knows the proper addresses. All we really care about
            is the length. But, these values are passed after this byte, first address then length */
            //really are supposed to respond with error codes if conditions aren't right but doing nothing
            //is faster to code for now. Also, need to validate returned length 
            if (buf[1] != 0) break; //NRC 0x70
            if (buf[2] != 0x44) break; //NRC 0x13
            if (!inSecurityMode) break; //this is NRC 0x33
            //don't care about the address but maybe validate it just to be 100% sure we're talking to the sane
            firmwareAddr = (buf[3] << 24) + (buf[4] << 16) + (buf[5] << 8) + (buf[6]);
            firmwareSize = (buf[7] << 24) + (buf[8] << 16) + (buf[9] << 8) + (buf[10]);
            //positive reply causes us to send the max acceptable payload info.
            sendBuffer[0] = UDS_REQUEST_DOWNLOAD + 0x40;
            sendBuffer[1] = 0x20; //16 bit reply with max packet size
            sendBuffer[2] = 0x1;
            sendBuffer[3] = 2;   //0x102 is 258 bytes.
            udsIsoTPTargetted.write(reply, sendBuffer, 4);
        break;
    case UDS_TRANSFER_DATA: //a chunk of firmware data
        //buf[1] has the block sequence counter which had better be going up by one each time. Must fault
        //if the counter goes out of sequence, otherwise, buf[2] - potentially buf[257] should be firmware data.
        //grab that data, keep track of our position in flash, and write it 256 bytes at a time.
        break;
    case UDS_REQUEST_TX_EXIT:
        break;
    default:
        break;
    }
}

UDSController::UDSController() : Device() {
    inSecurityMode = false;
    generatedSeed = false;
    commonName = "UDS Controller";
    shortName = "UDS";
}

void UDSController::earlyInit()
{
    prefsHandler = new PrefHandler(UDSCONTROLLER);
}

void UDSController::setup() {
    //TickHandler::getInstance()->detach(this);
    
    Entropy.Initialize();

    loadConfiguration();
    Device::setup();

    UDSConfiguration* config = (UDSConfiguration *)getConfiguration();

    cfgEntries.reserve(5);

    ConfigEntry entry;
    entry = {"UDS_RX", "Set CAN ID to receive UDS messages on", &config->udsRx, CFG_ENTRY_VAR_TYPE::UINT32, 0, 0x1FFFFFFFul, 16, nullptr};
    cfgEntries.push_back(entry);
    entry = {"UDS_TX", "Set CAN ID to send UDS messages on", &config->udsTx, CFG_ENTRY_VAR_TYPE::UINT32, 0, 0x1FFFFFFFul, 16, nullptr};
    cfgEntries.push_back(entry);
    entry = {"UDS_EXT", "Should extended addressing (29 bit IDs) be used? (0=No 1=Yes)", &config->useExtended, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"UDS_BROADCAST", "Should GEVCU listen on broadcast address? (0=No 1=Yes)", &config->listenBroadcast, CFG_ENTRY_VAR_TYPE::BYTE, 0, 1, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"UDS_BUS", "Listen on which bus? CAN1=1, CAN2=2, CAN3=3", &config->udsBus, CFG_ENTRY_VAR_TYPE::BYTE, 1, 3, 0, nullptr};
    cfgEntries.push_back(entry);

    udsIsoTPTargetted.begin();
    udsIsoTPTargetted.setBoundID(config->udsRx);
    udsIsoTPTargetted.setBoundBus(config->udsBus);
    udsIsoTPTargetted.setPadding(0xAA);

    switch (config->udsBus)
    {
    case 1:
        udsIsoTPTargetted.setWriteBus(&Can0);
        break;
    case 2:
        udsIsoTPTargetted.setWriteBus(&Can1);
        break;
    case 3:
        udsIsoTPTargetted.setWriteBus(&Can2);
        break;
    }

    udsIsoTPTargetted.onReceive(udsCallback);

    if (config->listenBroadcast)
    {
        udsIsoTPBroadcast.begin();
        udsIsoTPBroadcast.setBoundID(0x7DF);
        udsIsoTPBroadcast.setBoundBus(config->udsBus);
        switch (config->udsBus)
        {
        case 1:
            udsIsoTPBroadcast.setWriteBus(_CAN1);
            break;
        case 2:
            udsIsoTPBroadcast.setWriteBus(_CAN2);
            break;
        case 3:
            udsIsoTPBroadcast.setWriteBus(_CAN3);
            break;
        }
        udsIsoTPBroadcast.onReceive(udsCallback);
    }
    
/*    
    if (config->useExtended) canHandlerBus0.attach(this, config->udsRx, 0x1FFFFFFFul, true);
    else canHandlerBus0.attach(this, config->udsRx, 0x7FF, false);

    if (config->listenBroadcast)
    {
        canHandlerBus0.attach(this, 0x7DF, 0x7FF, false);
    }
*/
    //TickHandler::getInstance()->attach(this, CFG_TICK_INTERVAL_CAN_THROTTLE);
}

/*
 * There is no tick handler because this is the only place we do anything is here
*
	SAE standard says that this is the format for SAE requests to us:
	byte 0 = # of bytes following
	byte 1 = mode for PID request
	byte 2 = PID requested

	However, the sky is the limit for non-SAE frames (modes over 0x09)
	In that case we'll use two bytes for our custom PIDS (sent MSB first like
	all other PID traffic) MSB = byte 2, LSB = byte 3.

	These are the PIDs I think we should support (mode 1)
	0 = lets the other side know which pids we support. A bitfield that runs from MSb of first byte to lsb of last byte (32 bits)
	1 = Returns 32 bits but we really can only support the first byte which has bit 7 = Malfunction? Bits 0-6 = # of DTCs
	2 = Freeze DTC
	4 = Calculated engine load (A * 100 / 255) - Percentage
	5 = Engine Coolant Temp (A - 40) = Degrees Centigrade
	0x0C = Engine RPM (A * 256 + B) / 4
	0x11 = Throttle position (A * 100 / 255) - Percentage
	0x1C = Standard supported (We return 1 = OBDII)
	0x1F = runtime since engine start (A*256 + B)
	0x20 = pids supported (next 32 pids - formatted just like PID 0)
	0x21 = Distance traveled with fault light lit (A*256 + B) - In km
	0x2F = Fuel level (A * 100 / 255) - Percentage
	0x40 = PIDs supported, next 32
	0x51 = What type of fuel do we use? (We use 8 = electric, presumably.)
	0x60 = PIDs supported, next 32
	0x61 = Driver requested torque (A-125) - Percentage
	0x62 = Actual Torque delivered (A-125) - Percentage
	0x63 = Reference torque for engine - presumably max torque - A*256 + B - Nm

	Mode 3
	Returns DTC (diag trouble codes) - Three per frame
	bits 6-7 = DTC first character (00 = P = Powertrain, 01=C=Chassis, 10=B=Body, 11=U=Network)
	bits 4-5 = Second char (00 = 0, 01 = 1, 10 = 2, 11 = 3)
	bits 0-3 = third char (stored as normal nibble)
	Then next byte has two nibbles for the next 2 characters (fourth char = bits 4-7, fifth = 0-3)

	Mode 9 PIDs
	0x0 = Mode 9 pids supported (same scheme as mode 1)
	0x9 = How long is ECU name returned by 0x0A?
	0xA = ASCII string of ECU name. 20 characters are to be returned, I believe 4 at a time

 *
 */
void UDSController::handleCanFrame(const CAN_message_t &frame) 
{
}


//Process SAE standard PID requests. Function returns whether it handled the request or not.
bool UDSController::processShowData(const uint8_t *inData) {
    MotorController* motorController = deviceManager.getMotorController();
    int temp;

    switch (inData[1]) {
    case 0: //pids 1-0x20 that we support - bitfield
        //returns 4 bytes so immediately indicate that.
        sendBuffer[511] = 4;
        sendBuffer[2] = 0b11011000; //pids 1 - 8 - starting with pid 1 in the MSB and going from there
        sendBuffer[3] = 0b00010000; //pids 9 - 0x10
        sendBuffer[4] = 0b10000000; //pids 0x11 - 0x18
        sendBuffer[5] = 0b00010011; //pids 0x19 - 0x20
        return true;
        break;
    case 1: //Returns 32 bits but we really can only support the first byte which has bit 7 = Malfunction? Bits 0-6 = # of DTCs
        sendBuffer[511] = 4;
        sendBuffer[2] = 0; //TODO: We aren't properly keeping track of faults yet but when we do fix this.
        sendBuffer[3] = 0; //these next three are really related to ICE diagnostics
        sendBuffer[4] = 0; //so ignore them.
        sendBuffer[5] = 0;
        return true;
        break;
    case 2: //Freeze DTC
        return false; //don't support freeze framing yet. Might be useful in the future.
        break;
    case 4: //Calculated engine load (A * 100 / 255) - Percentage
        temp = (255 * motorController->getTorqueActual()) / motorController->getTorqueAvailable();
        sendBuffer[511] = 1;
        sendBuffer[2] = (uint8_t)(temp & 0xFF);
        return true;
        break;
    case 5: //Engine Coolant Temp (A - 40) = Degrees Centigrade
        //our code stores temperatures as a signed integer for tenths of a degree so translate
        temp =  motorController->getTemperatureSystem() / 10;
        if (temp < -40) temp = -40;
        if (temp > 215) temp = 215;
        temp += 40;
        sendBuffer[511] = 1; //returning only one byte
        sendBuffer[2] = (uint8_t)(temp);
        return true;
        break;
    case 0xC: //Engine RPM (A * 256 + B) / 4
        temp = motorController->getSpeedActual() * 4; //we store in RPM while the PID code wants quarter rpms
        sendBuffer[511] = 2;
        sendBuffer[2] = (uint8_t)(temp / 256);
        sendBuffer[3] = (uint8_t)(temp);
        return true;
        break;
    case 0x11: //Throttle position (A * 100 / 255) - Percentage
        temp = motorController->getThrottle() / 10; //getThrottle returns in 10ths of a percent
        if (temp < 0) temp = 0; //negative throttle can't be shown for OBDII
        temp = (255 * temp) / 100;
        sendBuffer[511] = 1;
        sendBuffer[2] = (uint8_t)(temp);
        return true;
        break;
    case 0x1C: //Standard supported (We return 1 = OBDII)
        sendBuffer[511] = 1;
        sendBuffer[2] = 1;
        return true;
        break;
    case 0x1F: //runtime since engine start (A*256 + B)
        sendBuffer[511] = 2;
        sendBuffer[2] = 0; //TODO: Get the actual runtime.
        sendBuffer[3] = 0;
        return true;
        break;
    case 0x20: //pids supported (next 32 pids - formatted just like PID 0)
        sendBuffer[511] = 4;
        sendBuffer[2] = 0b00000000; //pids 0x21 - 0x28 - starting with pid 0x21 in the MSB and going from there
        sendBuffer[3] = 0b00000000; //pids 0x29 - 0x30
        sendBuffer[4] = 0b00000000; //pids 0x31 - 0x38
        sendBuffer[5] = 0b00000001; //pids 0x39 - 0x40
        return true;
        break;
    case 0x21: //Distance traveled with fault light lit (A*256 + B) - In km
        sendBuffer[511] = 2;
        sendBuffer[2] = 0; //TODO: Can we get this information?
        sendBuffer[3] = 0;
        return true;
        break;
    case 0x2F: //Fuel level (A * 100 / 255) - Percentage
        sendBuffer[511] = 1;
        sendBuffer[2] = 0; //TODO: finish BMS interface and get this value
        return true;
        break;
    case 0x40: //PIDs supported, next 32
        sendBuffer[511] = 4;
        sendBuffer[2] = 0b00000000; //pids 0x41 - 0x48 - starting with pid 0x41 in the MSB and going from there
        sendBuffer[3] = 0b00000000; //pids 0x49 - 0x50
        sendBuffer[4] = 0b10000000; //pids 0x51 - 0x58
        sendBuffer[5] = 0b00000001; //pids 0x59 - 0x60
        return true;
        break;
    case 0x51: //What type of fuel do we use? (We use 8 = electric, presumably.)
        sendBuffer[511] = 1;
        sendBuffer[2] = 8;
        return true;
        break;
    case 0x60: //PIDs supported, next 32
        sendBuffer[511] = 4;
        sendBuffer[2] = 0b11100000; //pids 0x61 - 0x68 - starting with pid 0x61 in the MSB and going from there
        sendBuffer[3] = 0b00000000; //pids 0x69 - 0x70
        sendBuffer[4] = 0b00000000; //pids 0x71 - 0x78
        sendBuffer[5] = 0b00000000; //pids 0x79 - 0x80
        return true;
        break;
    case 0x61: //Driver requested torque (A-125) - Percentage
        temp = (100 * motorController->getTorqueRequested()) / motorController->getTorqueAvailable();
        temp += 125;
        sendBuffer[511] = 1;
        sendBuffer[2] = (uint8_t)temp;
        return true;
        break;
    case 0x62: //Actual Torque delivered (A-125) - Percentage
        temp = (100 * motorController->getTorqueActual()) / motorController->getTorqueAvailable();
        temp += 125;
        sendBuffer[511] = 1;
        sendBuffer[2] = (uint8_t)temp;
        return true;
        break;
    case 0x63: //Reference torque for engine - presumably max torque - A*256 + B - Nm
        temp = motorController->getTorqueAvailable();
        sendBuffer[511] = 2;
        sendBuffer[2] = (uint8_t)(temp / 256);
        sendBuffer[3] = (uint8_t)(temp & 0xFF);
        return true;
        break;
    }
    return false;
}

bool UDSController::processShowCustomData(const CAN_message_t &inFrame, CAN_message_t& outFrame) {
    int pid = inFrame.buf[2] * 256 + inFrame.buf[3];
    switch (pid) {
    }
    return false;
}

//grab 32 bits of truely random data from the Teensy processor
void UDSController::generateChallenge()
{
    *((uint32_t *)challenge) = Entropy.random();
    generatedSeed = true;
}

//expects a pointer to 4 bytes. Take our challenge, calculate the bytes and compare.
//due to the true RNG in the Teensy plus the tables being randomly generated this is
//probably pretty secure and hard to crack.
//Well, other than the fact that you're reading the source code...
//The bytes array sent here starts right where the 4 returned bytes are so everything lines up
bool UDSController::validateResponse(const uint8_t *bytes)
{
    uint8_t val; 
    Logger::debug("Validating security reply");
    for (int i = 0; i < 4; i++)
    {
        val = challenge[i];
        val = (val * multTable[i]) ^ xorTable[i];
        Logger::debug("Calc: %X   Passed Value %X", val, bytes[i]);
        if (val != bytes[i]) return false;
    }
    return true;
}

void UDSController::handleTick()
{

}

DeviceId UDSController::getId() {
    return UDSCONTROLLER;
}

void UDSController::loadConfiguration() {
    UDSConfiguration *config = (UDSConfiguration *) getConfiguration();

    if (!config) { // as lowest sub-class make sure we have a config object
        config = new UDSConfiguration();
        setConfiguration(config);
    }

    Device::loadConfiguration(); // call parent

    prefsHandler->read("udsRxID", &config->udsRx, 0x7E0);
    prefsHandler->read("udsTxID", &config->udsTx, 0x7E8);
    prefsHandler->read("udsUseExtended", &config->useExtended, 0);
    prefsHandler->read("udsBus", &config->udsBus, 1);
    prefsHandler->read("udsListenBroadcast", &config->listenBroadcast, 0);
}

/*
 * Store the current configuration to EEPROM
 */
void UDSController::saveConfiguration() {
    UDSConfiguration *config = (UDSConfiguration *) getConfiguration();

    Device::saveConfiguration(); // call parent

    prefsHandler->write("udsRxID", config->udsRx);
    prefsHandler->write("udsTxID", config->udsTx);
    prefsHandler->write("udsUseExtended", config->useExtended);
    prefsHandler->write("udsBus", config->udsBus);
    prefsHandler->write("udsListenBroadcast", config->listenBroadcast);
    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}


