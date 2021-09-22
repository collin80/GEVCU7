/*
 * VehicleSpecific.h - All code specific to your particular vehicle. This keeps the rest of code generic.
 *
 Copyright (c) 2016 Collin Kidder, Michael Neuweiler, Charles Galpin

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
 * The purpose of this file is to create a "device" that is used to interface with all of the other enabled
 * devices in order to create a custom control system for your car. Each main device has configuration options
 * and is meant for generalized usage. But, what if you want to go further? Perhaps you've got a PowerKeyPro
 * keypad and you want to be able to select different regen powers or drive power settings? The default device
 * drivers don't support such things because they're generic. Or, perhaps you want to allow
 * your BMS and motor controller to talk? Maybe the BMS can limit regen or drive power automatically based on battery
 * conditions. The code doesn't currently support such things but it would be possible to read the BMS here and then update
 * the motor controller. The options are endless. The below code will be updated to show what Collin has done with a reference car
 * in order to show what is possible and how it could be coded. Feel free to delete any code not appropriate to your car and/or
 * add any extra code you need to make GEVCU do what you want.
 * This is a device like any other so it is disabled by default and will have to be enabled if you need to use it.
*/

#include "VehicleSpecific.h"

/*
 * Constructor
 */
VehicleSpecific::VehicleSpecific() : Device() {    
    commonName = "VehicleSpecific";
    shortName = "Vehicle";
    didInitialSetup = false;
    waitTicksStartup = (5000000ul / CFG_TICK_INTERVAL_VEHICLE);
}

void VehicleSpecific::earlyInit()
{
    prefsHandler = new PrefHandler(VEHICLESPECIFIC);
}

/*
 * Setup the device.
 */
void VehicleSpecific::setup() {
    tickHandler.detach(this); // unregister from TickHandler first

    Logger::info("add device: VehicleSpecific (id: %X, %X)", VEHICLESPECIFIC, this);

    loadConfiguration();

    Device::setup(); //call base class

    //Use same tick interval as a pot based pedal would have used.
    tickHandler.attach(this, CFG_TICK_INTERVAL_VEHICLE);
}

/*
 * Process a timer event. This is where you should be doing checks and updates. By default this
 * function is called 10 times per second.
 */
void VehicleSpecific::handleTick() {
    Device::handleTick(); // Call parent which controls the workflow
    Logger::debug("VS Tick Handler");

    MotorController * motor = deviceManager.getMotorController();

    if (waitTicksStartup > 0) 
    {
        waitTicksStartup--;
        return;
    }
    
    if (!didInitialSetup)
    {
        didInitialSetup = true;
        systemIO.setAnalogOut(1, 1); //set second button's LED to RED
        systemIO.setAnalogOut(0, 1000); //send batch
        if (motor) motor->setSelectedGear(MotorController::NEUTRAL);
    }
    
    //button 0 on the pad is reverse
    //button 1 is neutral
    //button 2 is drive
    //button 3 is status
    //button 4 is cruise set / minus
    //button 5 is cruise resume / plus
    //button 6 is turtle mode
    //button 7 is rabbit mode
    //There are four digital inputs before us (0-3) so buttons start at digital input 4
    if (systemIO.getDigitalIn(5)) //set drive mode neutral
    {
        Logger::debug("VS Setting gear to neutral");
        systemIO.setAnalogOut(0, 0);
        systemIO.setAnalogOut(1, 1);
        systemIO.setAnalogOut(2, 0);
        systemIO.setAnalogOut(0, 1000);
        if (motor) motor->setSelectedGear(MotorController::NEUTRAL);
    }
    if (systemIO.getDigitalIn(6)) //set drive mode to drive
    {
        Logger::debug("VS Setting gear to drive");
        systemIO.setAnalogOut(0, 0);
        systemIO.setAnalogOut(1, 0);
        systemIO.setAnalogOut(2, 1);        
        systemIO.setAnalogOut(0, 1000);
        if (motor) motor->setSelectedGear(MotorController::DRIVE);
    }
    if (systemIO.getDigitalIn(4)) //set drive mode to reverse
    {
        Logger::debug("VS Setting gear to reverse");
        systemIO.setAnalogOut(0, 1);
        systemIO.setAnalogOut(1, 0);
        systemIO.setAnalogOut(2, 0);        
        systemIO.setAnalogOut(0, 1000);
        if (motor) motor->setSelectedGear(MotorController::REVERSE);
    }
    
}

/*
 * Return the device ID
 */
DeviceId VehicleSpecific::getId() {
    return (VEHICLESPECIFIC);
}

DeviceType VehicleSpecific::getType()
{
    return DEVICE_MISC;
}

/*
 * Load the device configuration.
 * If possible values are read from EEPROM. If not, reasonable default values
 * are chosen and the configuration is overwritten in the EEPROM.
 */
void VehicleSpecific::loadConfiguration() {

    Device::loadConfiguration(); // call parent
}

/*
 * Store the current configuration to EEPROM
 */
void VehicleSpecific::saveConfiguration() {

    Device::saveConfiguration(); // call parent
}

VehicleSpecific vehicle;
