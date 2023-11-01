/*
 * CanThrottle.cpp
 *
Copyright (c) 2013 Collin Kidder, Michael Neuweiler, Charles Galpin

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

#include "CanThrottle.h"

CanThrottle::CanThrottle() : Throttle() {
    rawSignal.input1 = 0;
    rawSignal.input2 = 0;
    rawSignal.input3 = 0;
    ticksNoResponse = 255; // invalidate input signal until response is received
    responseId = 0;
    responseMask = 0x7ff;
    responseExtended = false;

    commonName = "CANBus accelerator";
    shortName = "CANAccel";
}

void CanThrottle::earlyInit()
{
    prefsHandler = new PrefHandler(CANACCELPEDAL);
}

void CanThrottle::setup() {
    tickHandler.detach(this);

    Logger::info("add device: CanThrottle (id: %X, %X)", CANACCELPEDAL, this);

    loadConfiguration();
    Throttle::setup();

    CanThrottleConfiguration *config = (CanThrottleConfiguration *)getConfiguration();

    ConfigEntry entry;
    //        cfgName                 helpText                               variable ref        Type                   Min Max Precision Funct
    entry = {"CANTHROT-CANBUS", "Set which CAN bus to connect to (0-2)", &config->canbusNum, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"CANTHROT-CARTYPE", "Set CAN pedal type (1=Volvo S80 Gasoline, 2=Volvo V50 Diesel)", &config->carType, CFG_ENTRY_VAR_TYPE::BYTE, 0, 2, 0, nullptr};
    cfgEntries.push_back(entry);

    setAttachedCANBus(config->canbusNum);

    requestFrame.len = 0x08;
    requestFrame.flags.extended = 0x00;

    switch (config->carType) {
    case Volvo_S80_Gas:
        // Request: dlc=0x08 fid=0x7e0 id=0x7e0 ide=0x00 rtr=0x00 data=0x03,0x22,0xEE,0xCB,0x00,0x00,0x00,0x00 (vida: [0x00, 0x00, 0x07, 0xe0, 0x22, 0xee, 0xcb])
        // Raw response: dlc=0x08 fid=0x7e8 id=0x7e8 ide=0x00 rtr=0x00 data=0x04,0x62,0xEE,0xCB,0x14,0x00,0x00,0x00 (vida: [0x00, 0x00, 0x07, 0xe8, 0x62, 0xee, 0xcb, 0x14])
        requestFrame.id = 0x7e0;
        memcpy(requestFrame.buf, (const uint8_t[]) {
            0x03, 0x22, 0xee, 0xcb, 0x00, 0x00, 0x00, 0x00
        }, 8);
        responseId = 0x7e8;
        break;
    case Volvo_V50_Diesel:
        // Request: dlc=0x08 fid=0xFFFFE id=0x3FFFE ide=0x01 rtr=0x00 data=0xCD,0x11,0xA6,0x00,0x24,0x01,0x00,0x00 ([0x00, 0xf, 0xff, 0xfe, 0xcd, 0x11, 0xa6, 0x00, 0x24, 0x01, 0x00, 0x00])
        // Response: dlc=0x08 fid=0x400021 id=0x21 ide=0x01 rtr=0x00 data=0xCE,0x11,0xE6,0x00,0x24,0x03,0xFD,0x00 (vida: [0x00, 0x40, 0x00, 0x21, 0xce, 0x11, 0xe6, 0x00, 0x24, 0x03, 0xfd, 0x00])
        requestFrame.id = 0x3FFFE;
        requestFrame.flags.extended = 0x01;
        memcpy(requestFrame.buf, (const uint8_t[]) {
            0xce, 0x11, 0xe6, 0x00, 0x24, 0x03, 0xfd, 0x00
        }, 8);
        responseId = 0x21;
        responseExtended = true;
        break;
    default:
        Logger::error(CANACCELPEDAL, "no valid car type defined.");
    }

    attachedCANBus->attach(this, responseId, responseMask, responseExtended);
    tickHandler.attach(this, CFG_TICK_INTERVAL_CAN_THROTTLE);
}

/*
 * Send a request to the ECU.
 *
 */
void CanThrottle::handleTick() {
    Throttle::handleTick(); // Call parent handleTick

    attachedCANBus->sendFrame(requestFrame);

    if (ticksNoResponse < 255) // make sure it doesn't overflow
        ticksNoResponse++;
}

/*
 * Handle the response of the ECU and calculate the throttle value
 *
 */
void CanThrottle::handleCanFrame(const CAN_message_t &frame) {
    CanThrottleConfiguration *config = (CanThrottleConfiguration *)getConfiguration();

    if (frame.id == responseId) {
        switch (config->carType) {
        case Volvo_S80_Gas:
            rawSignal.input1 = frame.buf[4];
            break;
        case Volvo_V50_Diesel:
            rawSignal.input1 = (frame.buf[5] + 1) * frame.buf[6];
            break;
        }
        ticksNoResponse = 0;
    }
}

RawSignalData* CanThrottle::acquireRawSignal() {
    return &rawSignal; // should have already happened in the background
}

bool CanThrottle::validateSignal(RawSignalData* rawSignal) {
    CanThrottleConfiguration *config = (CanThrottleConfiguration *) getConfiguration();

    if (ticksNoResponse >= CFG_CANTHROTTLE_MAX_NUM_LOST_MSG) {
        if (status == OK)
            Logger::error(CANACCELPEDAL, "no response on position request received: %d ", ticksNoResponse);
        status = ERR_MISC;
        return false;
    }
    if (rawSignal->input1 > (config->maximumLevel1 + CFG_THROTTLE_TOLERANCE)) {
        if (status == OK)
            Logger::error(CANACCELPEDAL, (char *)Constants::valueOutOfRange, rawSignal->input1);
        status = ERR_HIGH_T1;
        return false;
    }
    if (rawSignal->input1 < (config->minimumLevel1 - CFG_THROTTLE_TOLERANCE)) {
        if (status == OK)
            Logger::error(CANACCELPEDAL, (char *)Constants::valueOutOfRange, rawSignal->input1);
        status = ERR_LOW_T1;
        return false;
    }

    // all checks passed -> throttle seems to be ok
    if (status != OK)
        Logger::info(CANACCELPEDAL, (char *)Constants::normalOperation);
    status = OK;
    return true;
}

int16_t CanThrottle::calculatePedalPosition(RawSignalData* rawSignal) {
    CanThrottleConfiguration *config = (CanThrottleConfiguration *) getConfiguration();

    return normalizeAndConstrainInput(rawSignal->input1, config->minimumLevel1, config->maximumLevel1);
}

DeviceId CanThrottle::getId() {
    return CANACCELPEDAL;
}

void CanThrottle::loadConfiguration() {
    CanThrottleConfiguration *config = (CanThrottleConfiguration *) getConfiguration();

    if (!config) { // as lowest sub-class make sure we have a config object
        config = new CanThrottleConfiguration();
        setConfiguration(config);
    }

    Throttle::loadConfiguration(); // call parent


    //if (prefsHandler->checksumValid()) { //checksum is good, read in the values stored in EEPROM
        Logger::debug(CANACCELPEDAL, (char *)Constants::validChecksum);
        prefsHandler->read("ThrottleMin1", &config->minimumLevel1, 400);
        prefsHandler->read("ThrottleMax1", &config->maximumLevel1, 1800);
        prefsHandler->read("ThrottleCarType", &config->carType, Volvo_S80_Gas);
        prefsHandler->read("CanbusNum", &config->canbusNum, 1);
    Logger::debug(CANACCELPEDAL, "T1 MIN: %i MAX: %i Type: %d", config->minimumLevel1, config->maximumLevel1, config->carType);
}

/*
 * Store the current configuration to EEPROM
 */
void CanThrottle::saveConfiguration() {
    CanThrottleConfiguration *config = (CanThrottleConfiguration *) getConfiguration();

    prefsHandler->write("ThrottleMin1", config->minimumLevel1);
    prefsHandler->write("ThrottleMax1", config->maximumLevel1);
    prefsHandler->write("ThrottleCarType", config->carType);
    prefsHandler->write("CanbusNum", config->canbusNum);
    prefsHandler->saveChecksum();
    Throttle::saveConfiguration(); // call parent
}

//auto register thineself
CanThrottle canThrottle;
