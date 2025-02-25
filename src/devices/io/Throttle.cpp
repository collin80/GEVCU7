/*
 * Throttle.cpp
 *
 * Parent class for all throttle controllers

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

#include "Throttle.h"
#include "../../DeviceManager.h"

/*
 * Constructor
 */
Throttle::Throttle() : Device() {
    level = 0;
    status = OK;
    deviceType = DEVICE_THROTTLE;
}

void Throttle::earlyInit()
{

}

void Throttle::setup()
{
    ThrottleConfiguration *config = (ThrottleConfiguration *) getConfiguration();
    cfgEntries.reserve(20);

    ConfigEntry entry = {"TRNGMAX", "Tenths of a percent of pedal where regen is at max", &config->positionRegenMaximum, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TRNGMIN", "Tenths of a percent of pedal where regen is at min", &config->positionRegenMinimum, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TFWD", "Tenths of a percent of pedal where forward motion starts", &config->positionForwardMotionStart, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr};
    cfgEntries.push_back(entry);
    //entry = {"TMAP", "Tenths of a percent of pedal where 50% throttle will be", &config->positionHalfPower, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr};
    entry = {"TMAP1IN", "Tenths of a percent of pedal input where first map point is", &config->mapPoints[0].inputPosition, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TMAP1OUT", "Tenths of a percent of throttle output where first map point is", &config->mapPoints[0].outputPosition, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TMAP2IN", "Tenths of a percent of pedal input where second map point is", &config->mapPoints[1].inputPosition, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TMAP2OUT", "Tenths of a percent of throttle output where second map point is", &config->mapPoints[1].outputPosition, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TMAP3IN", "Tenths of a percent of pedal input where third map point is", &config->mapPoints[2].inputPosition, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TMAP3OUT", "Tenths of a percent of throttle output where third map point is", &config->mapPoints[2].outputPosition, CFG_ENTRY_VAR_TYPE::UINT16, 0, 1000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TMINRN", "Percent of full torque to use for min throttle regen", &config->minimumRegen, CFG_ENTRY_VAR_TYPE::BYTE, 0, 100, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TMAXRN", "Percent of full torque to use for max throttle regen", &config->maximumRegen, CFG_ENTRY_VAR_TYPE::BYTE, 0, 100, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"TCREEP", "Percent of full torque to use for creep (0=disable)", &config->creep, CFG_ENTRY_VAR_TYPE::BYTE, 0, 100, 0, nullptr};
    cfgEntries.push_back(entry);

    StatusEntry stat;
    //        name              var         type             prevVal  obj
    stat = {"Throttle_Level", &level, CFG_ENTRY_VAR_TYPE::INT16, 0, this};
    deviceManager.addStatusEntry(stat);
}


/*
 * Controls the main flow of throttle data acquisiton, validation and mapping to
 * user defined behaviour.
 *
 * Get's called by the sub-class which is triggered by the tick handler
 */
void Throttle::handleTick() {
    Device::handleTick();

    RawSignalData *rawSignals = acquireRawSignal(); // get raw data from the throttle device
    if (validateSignal(rawSignals)) { // validate the raw data
        int16_t position = calculatePedalPosition(rawSignals); // bring the raw data into a range of 0-1000 (without mapping)
        level = mapPedalPosition(position); // apply mapping of the 0-1000 range to the user defined settings
    } else
        level = 0;
}

/*
 * Maps the input throttle position (0-1000 permille) to an output level which is
 * calculated based on the throttle mapping parameters (free float, regen, acceleration,
 * 50% acceleration).
 * The output value will be in the range of -1000 to 1000. The value will be used by the
 * MotorController class to calculate commanded torque or speed. Positive numbers result in
 * acceleration, negative numbers in regeneration. 0 will result in no force applied by
 * the motor.
 *
 * Configuration parameters:
 * positionRegenMaximum: The pedal position (0-1000) where maximumRegen will be applied. If not 0, then
 *                       moving the pedal from positionRegenMaximum to 0 will result in linear reduction
 *                       of regen from maximumRegen.
 * positionRegenMinimum: The pedal position (0-1000) where minimumRegen will be applied. If not 0, then
 *                       a linear regen force will be applied when moving the pedal from positionRegenMinimum
 *                       to positionRegenMaximum.
 * positionForwardMotionStart: The pedal position where the car starts to accelerate. If not equal to
 *                       positionRegenMinimum, then the gap between the two positions will result in no force
 *                       applied (free coasting).
 * positionHalfPower:    Position of the pedal where 50% of the maximum torque will be applied. To gain more
 *                       fine control in the lower speed range (e.g. when parking) it might make sense to
 *                       set this position higher than the mid point of positionForwardMotionStart and full
 *                       throttle.
 *
 * Important pre-condition (to be checked when changing parameters) :
 * 0 <= positionRegenMaximum <= positionRegenMinimum <= positionForwardMotionStart <= positionHalfPower
 */
int16_t Throttle::mapPedalPosition(int16_t pedalPosition) {
    int16_t throttleLevel, range, value;
    ThrottleConfiguration *config = (ThrottleConfiguration *) getConfiguration();

    throttleLevel = 0;

    if (pedalPosition == 0 && config->creep > 0) {
        throttleLevel = 10 * config->creep;
    } else if (pedalPosition <= config->positionRegenMinimum) {
        if (pedalPosition >= config->positionRegenMaximum) {
            range = config->positionRegenMinimum - config->positionRegenMaximum;
            value = pedalPosition - config->positionRegenMaximum;
            if (range != 0) // prevent div by zero, should result in 0 throttle if min==max
                throttleLevel = -10 * config->minimumRegen + (config->maximumRegen - config->minimumRegen) * (100 - value * 100 / range) / -10;
        } else {
            // no ramping yet below positionRegenMaximum, just drop to 0
//			range = config->positionRegenMaximum;
//			value = pedalPosition;
//			throttleLevel = -10 * config->maximumRegen * value / range;
        }
    }

    if (pedalPosition >= config->positionForwardMotionStart) {
        //start of throttle map up to first map point
        throttleLevel = map(pedalPosition, config->positionForwardMotionStart, config->mapPoints[0].inputPosition, 
                                0, config->mapPoints[0].outputPosition);

        if ( (pedalPosition >= config->mapPoints[0].inputPosition) && (config->mapPoints[1].inputPosition > config->mapPoints[0].inputPosition) ) 
            throttleLevel = map(pedalPosition, config->mapPoints[0].inputPosition, config->mapPoints[1].inputPosition, 
                                config->mapPoints[0].outputPosition, config->mapPoints[1].outputPosition);

        if ( (pedalPosition >= config->mapPoints[1].inputPosition) && (config->mapPoints[2].inputPosition > config->mapPoints[1].inputPosition) ) 
            throttleLevel = map(pedalPosition, config->mapPoints[1].inputPosition, config->mapPoints[2].inputPosition, 
                                config->mapPoints[1].outputPosition, config->mapPoints[2].outputPosition);

        if ( (pedalPosition >= config->mapPoints[2].inputPosition) && (config->mapPoints[2].inputPosition < 1000) ) 
            throttleLevel = map(pedalPosition, config->mapPoints[2].inputPosition, 1000, 
                                config->mapPoints[2].outputPosition, 1000);


        /*if (pedalPosition <= config->positionHalfPower) {
            range = config->positionHalfPower - config->positionForwardMotionStart;
            value = pedalPosition - config->positionForwardMotionStart;
            if (range != 0) // prevent div by zero, should result in 0 throttle if half==startFwd
                throttleLevel = 500 * value / range;
        } else {
            range = 1000 - config->positionHalfPower;
            value = pedalPosition - config->positionHalfPower;
            throttleLevel = 500 + 500 * value / range;
        }*/

    }
    //Logger::debug("throttle level: %d", throttleLevel);
    
    //check to see if an invalid throttle level was generated by some fluke. Do not accept this condition!
    if (throttleLevel < -1050) {        
        Logger::error("Generated throttle level (%i) was way too low!", throttleLevel);
        throttleLevel = 0;
        status = ERR_MISC;
        
    }
    if (throttleLevel > 1050) {
        Logger::error("Generated throttle level (%i) was way too high!", throttleLevel);
        throttleLevel = 0;
        status = ERR_MISC;
    }

    //A bit of a kludge. Normally it isn't really possible to ever get to
    //100% output. This next line just fudges the numbers a bit to make it
    //more likely to get that last bit of power
    if (throttleLevel > 979) throttleLevel = 1000;

    return throttleLevel;
}

/*
 * Make sure input level stays within margins (min/max) then map the constrained
 * level linearly to a value from 0 to 1000.
 */
int16_t Throttle::normalizeAndConstrainInput(int32_t input, int32_t min, int32_t max) {
    return constrain(normalizeInput(input, min, max), (int32_t) 0, (int32_t) 1000);
}

/*
 * Map the constrained level linearly to a signed value from 0 to 1000.
 */
int32_t Throttle::normalizeInput(int32_t input, int32_t min, int32_t max) {
    return map(input, min, max, (int32_t) 0, (int32_t) 1000);
}

/*
 * Returns the currently calculated/mapped throttle level (from -1000 to 1000).
 */
int16_t Throttle::getLevel() {
    return level;
}

/*
 * Return the throttle's current status
 */
Throttle::ThrottleStatus Throttle::getStatus() {
    return status;
}

/*
 * Is the throttle faulted?
 */
bool Throttle::isFaulted() {
    return status != OK;
}

RawSignalData* Throttle::acquireRawSignal() {
    return NULL;
}

bool Throttle::validateSignal(RawSignalData*) {
    return false;
}

int16_t Throttle::calculatePedalPosition(RawSignalData*) {
    return 0;
}

/*
 * Load the config parameters which are required by all throttles
 */
void Throttle::loadConfiguration() {
    ThrottleConfiguration *config = (ThrottleConfiguration *) getConfiguration();

    Device::loadConfiguration(); // call parent

    //if (prefsHandler->checksumValid()) { //checksum is good, read in the values stored in EEPROM
        prefsHandler->read("RegenMin", &config->positionRegenMinimum, 270);
        prefsHandler->read("RegenMax", &config->positionRegenMaximum, 0);
        prefsHandler->read("ForwardStart", &config->positionForwardMotionStart, 280);
        //prefsHandler->read("MapPoint", &config->positionHalfPower, 750);
        prefsHandler->read("MapPoint1I", &config->mapPoints[0].inputPosition, 750);
        prefsHandler->read("MapPoint1O", &config->mapPoints[0].outputPosition, 500);
        prefsHandler->read("MapPoint2I", &config->mapPoints[1].inputPosition, 750);
        prefsHandler->read("MapPoint2O", &config->mapPoints[1].outputPosition, 500);
        prefsHandler->read("MapPoint3I", &config->mapPoints[2].inputPosition, 750);
        prefsHandler->read("MapPoint3O", &config->mapPoints[2].outputPosition, 500);
        prefsHandler->read("Creep", &config->creep, 0);
        prefsHandler->read("MinAccelRegen", &config->minimumRegen, 0);
        prefsHandler->read("MaxAccelRegen", &config->maximumRegen, 70);
    
    //Logger::debug(THROTTLE, "RegenMax: %i RegenMin: %i Fwd: %i Map: %i", config->positionRegenMaximum, config->positionRegenMinimum,
    //              config->positionForwardMotionStart, config->positionHalfPower);
    Logger::debug(THROTTLE, "MinRegen: %d MaxRegen: %d", config->minimumRegen, config->maximumRegen);
}

/*
 * Store the current configuration to EEPROM
 */
void Throttle::saveConfiguration() {
    ThrottleConfiguration *config = (ThrottleConfiguration *) getConfiguration();

    Device::saveConfiguration(); // call parent

    prefsHandler->write("RegenMin", config->positionRegenMinimum);
    prefsHandler->write("RegenMax", config->positionRegenMaximum);
    prefsHandler->write("ForwardStart", config->positionForwardMotionStart);
    //prefsHandler->write("MapPoint", config->positionHalfPower);
    prefsHandler->write("MapPoint1I", config->mapPoints[0].inputPosition);
    prefsHandler->write("MapPoint1O", config->mapPoints[0].outputPosition);
    prefsHandler->write("MapPoint2I", config->mapPoints[1].inputPosition);
    prefsHandler->write("MapPoint2O", config->mapPoints[1].outputPosition);
    prefsHandler->write("MapPoint3I", config->mapPoints[2].inputPosition);
    prefsHandler->write("MapPoint3O", config->mapPoints[2].outputPosition);
    prefsHandler->write("Creep", config->creep);
    prefsHandler->write("MinAccelRegen", config->minimumRegen);
    prefsHandler->write("MaxAccelRegen", config->maximumRegen);
    prefsHandler->saveChecksum();
    prefsHandler->forceCacheWrite();

    Logger::console("Throttle configuration saved");
}


