/*
 * TestThrottle.cpp
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

#include "TestThrottle.h"

/*
 * Constructor
 */
TestThrottle::TestThrottle() : Throttle() {    
    commonName = "Test/Debug Accelerator";
    shortName = "TestAccel";
    rampingDirection = true;
    rawSignal.input1 = 0;
    deviceId = TESTACCEL;
}

void TestThrottle::earlyInit()
{
    prefsHandler = new PrefHandler(TESTACCEL);
}

/*
 * Setup the device.
 */
void TestThrottle::setup() {
    tickHandler.detach(this); // unregister from TickHandler first

    Logger::info("add device: TestThrottle (id: %X, %X)", TESTACCEL, this);

    loadConfiguration();

    Throttle::setup(); //call base class

    //Use same tick interval as a pot based pedal would have used.
    tickHandler.attach(this, CFG_TICK_INTERVAL_POT_THROTTLE);
}

/*
 * Process a timer event.
 */
void TestThrottle::handleTick() {
    Throttle::handleTick(); // Call parent which controls the workflow
}

/*
 * Retrieve raw input signals from the throttle hardware.
 */
RawSignalData *TestThrottle::acquireRawSignal() {
    TestThrottleConfiguration *config = (TestThrottleConfiguration *) getConfiguration();

    if (rampingDirection) rawSignal.input1 += 1;
    else rawSignal.input1 -= 1;
    
    if (rawSignal.input1 <= config->minimumLevel1) {
        rawSignal.input1 = config->minimumLevel1;
        rampingDirection = true;
    }
    if (rawSignal.input1 >= config->maximumLevel1) {
        rawSignal.input1 = config->maximumLevel1;
        rampingDirection = false;
    }
    
    rawSignal.input2 = 0;
    return &rawSignal;
}

/*
 * Perform sanity check on the ADC input values. The values are normalized (without constraining them)
 * and the checks are performed on a 0-1000 scale with a percentage tolerance
 */
bool TestThrottle::validateSignal(RawSignalData *rawSignal) {
    TestThrottleConfiguration *config = (TestThrottleConfiguration *) getConfiguration();
    int32_t calcThrottle1;

    calcThrottle1 = normalizeInput(rawSignal->input1, config->minimumLevel1, config->maximumLevel1);

    if (calcThrottle1 > (1000 + CFG_THROTTLE_TOLERANCE))
    {
        if (status == OK)
            Logger::error(TESTACCEL, "ERR_HIGH_T1: throttle 1 value out of range: %i", calcThrottle1);
        status = ERR_HIGH_T1;
        faultHandler.raiseFault(TESTACCEL, FAULT_THROTTLE_HIGH_A);
        return false;
    }
    else
    {
        faultHandler.cancelOngoingFault(TESTACCEL, FAULT_THROTTLE_HIGH_A);
    }

    if (calcThrottle1 < (0 - CFG_THROTTLE_TOLERANCE)) {
        if (status == OK)
            Logger::error(TESTACCEL, "ERR_LOW_T1: throttle 1 value out of range: %i ", calcThrottle1);
        status = ERR_LOW_T1;
        faultHandler.raiseFault(TESTACCEL, FAULT_THROTTLE_LOW_A);
        return false;
    }
    else
    {
        faultHandler.cancelOngoingFault(TESTACCEL, FAULT_THROTTLE_LOW_A);
    }

    // all checks passed -> throttle is ok
    if (status != OK)
        Logger::info(TESTACCEL, (char *)Constants::normalOperation);
    status = OK;
    return true;
}

/*
 * Convert the raw ADC values to a range from 0 to 1000 (per mille) according
 * to the specified range and the type of potentiometer.
 */
int16_t TestThrottle::calculatePedalPosition(RawSignalData *rawSignal) {
    TestThrottleConfiguration *config = (TestThrottleConfiguration *) getConfiguration();
    uint16_t calcThrottle1;

    calcThrottle1 = normalizeInput(rawSignal->input1, config->minimumLevel1, config->maximumLevel1);

    return calcThrottle1;
}

/*
 * Load the device configuration.
 * If possible values are read from EEPROM. If not, reasonable default values
 * are chosen and the configuration is overwritten in the EEPROM.
 */
void TestThrottle::loadConfiguration() {
    TestThrottleConfiguration *config = (TestThrottleConfiguration *) getConfiguration();

    if (!config) { // as lowest sub-class make sure we have a config object
        config = new TestThrottleConfiguration();
        setConfiguration(config);
    }

    Throttle::loadConfiguration(); // call parent

    //if (prefsHandler->checksumValid()) { //checksum is good, read in the values stored in EEPROM
        Logger::debug(TESTACCEL, (char *)Constants::validChecksum);
        prefsHandler->read("ThrottleMin1", &config->minimumLevel1, 100);
        prefsHandler->read("ThrottleMax1", &config->maximumLevel1, 1700);

    Logger::debug(TESTACCEL, "T1 MIN: %i MAX: %i", config->minimumLevel1, config->maximumLevel1);
}

/*
 * Store the current configuration to EEPROM
 */
void TestThrottle::saveConfiguration() {
    TestThrottleConfiguration *config = (TestThrottleConfiguration *) getConfiguration();

    Throttle::saveConfiguration(); // call parent

    prefsHandler->write("ThrottleMin1", config->minimumLevel1);
    prefsHandler->write("ThrottleMax1", config->maximumLevel1);
    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();
}

DMAMEM TestThrottle testThrottle;
