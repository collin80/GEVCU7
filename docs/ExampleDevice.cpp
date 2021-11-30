/*
* Example.cpp - Implementation of the example device class
 
 Copyright (c) 2021 Collin Kidder

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

#include "ExampleDevice.h"

/*
 * Constructor
 */
Example::Example() : Device() 
{    
    commonName = "Example device"; //set commonName to a reasonable description of what your device is meant to do and/or support
    shortName = "ExampleDev"; //A shorter version used by various areas (like logging) just to tell you which device did something
    //you can initialize variable values here for your own things
}

void Example::earlyInit()
{
    //basically all you should do in earlyInit is to create the preference handler with your ID that was
    //setup in the header
    prefsHandler = new PrefHandler(EXAMPLE);
}

/*
 * Setup the device. Called only if the device is enabled. Do the real setup here so that it doesn't happen
   unless the device is really enabled.
 */ 
void Example::setup() 
{
    tickHandler.detach(this); // unregister from TickHandler first - just to be 100% sure we're in a good state

    Logger::info("add device: Example (id: %X, %X)", EXAMPLE, this);

    loadConfiguration(); //always load your configuration

    Device::setup(); //call base class 

    //get a local variable with the configuration items
    ExampleConfiguration *config = (ExampleConfiguration *)getConfiguration();

    //cfgEntries is used to specify all of the things you want the end user to be able to configure. 
    //Only the base class for a given device type will have the reserve line below. If you are
    //instead inheriting from something like the Throttle or MotorController class then they've already
    //called this for you.
    cfgEntries.reserve(3); //should reserve enough space for all the entries we plan to have

    ConfigEntry entry;
    //        Variable Name
    //        |           Description
    //        |           |                        Pointer to the config entry to use
    //        |           |                        |                    Type of variable this is
    //        |           |                        |                    |                         Min Value to accept
    //        |           |                        |                    |                         |  Max value
    //        |           |                        |                    |                         |  |  # of decimal places
    //        |           |                        |                    |                         |  |  |  Function that returns a desc of value                   
    entry = {"EX-FIRST", "First example variable", &config->firstValue, CFG_ENTRY_VAR_TYPE::BYTE, 0, 3, 0, DEV_PTR(&Example::describeFirstVar)};
    cfgEntries.push_back(entry);
    entry = {"EX-SECOND", "Second, bigger example variable", &config->secondValue, CFG_ENTRY_VAR_TYPE::UINT16, 0, 26000, 0, nullptr};
    cfgEntries.push_back(entry);
    entry = {"EX-FLOAT", "Decimal example variable", &config->fractionalValue, CFG_ENTRY_VAR_TYPE::FLOAT, -10.0f, 10.0f, 1, nullptr};
    cfgEntries.push_back(entry);

    /*
    The variable name is what the end user will use to configure that variable. For instance, EX-FIRST=5 to set the firstValue
    variable to a value of 5. The value will be constrained between the min and max values and will not be updated unless
    it falls within the allowable range. There are many variable types. You need to make sure the variable type you specify
    properly matches the actual variable you have passed in. If you are using a float number (can store numbers that aren't integer)
    then the # of decimal places can be specified as well. This is ignored for integers. Finally, the real mystery is that
    function pointer you can pass at the end. If your variables mean something discrete you can use this function to turn 
    those values into some text. In the example above we specify that our describeFirstVar function should be used for this.
    Below you will find that function. The function must return String and has no parameters. Instead it operates directly
    on the object's members so each individual ConfigEntry that you want to describe needs a separate function to describe it.
    Most ConfigEntry's probably don't need this functionality so nullptr is acceptable most of the time.
    */
    
    //if you need to periodically do things then attach to the tickHandler. You give it the number of microseconds between ticks
    //it is best if you use the same value as other devices so they can share a timer. But, the hardware has a lot of timers anyway
    tickHandler.attach(this, CFG_TICK_INTERVAL_EXAMPLE);
}

//given as a parameter to a ConfigEntry above. This function looks at our configuration item called firstValue and returns
//a unique string to describe each value. Technically you can use any variable and determine the resulting string any way
//you like so you can even describe floating point values, perhaps by range. It's up to you.
String Example::describeFirstVar()
{
    ExampleConfiguration *config = (ExampleConfiguration *)getConfiguration();
    //above we said that 0 is the minimum value and 3 is the maximum. So, that's the whole range
    if (config->firstValue == 0) return String("HAM");
    if (config->firstValue == 1) return String("STEAK");
    if (config->firstValue == 2) return String("CHILI");
    if (config->firstValue == 3) return String("BUFFALO");
    return String("Invalid!"); //just in case the value somehow got corrupted or otherwise saved out of range.
}

/*
 * Process a timer event. This is where you should be doing checks and updates. 
 */
void Example::handleTick() 
{
    Device::handleTick(); // Call parent which controls the workflow
    //Logger::debug("Example Tick Handler"); //for debugging you might uncomment this.

    ExampleConfiguration *config = (ExampleConfiguration *) getConfiguration();
}

/*
 * Return the device ID
 */
DeviceId Example::getId() 
{
    return (EXAMPLE);
}

DeviceType Example::getType()
{
    return DEVICE_MISC; //set to the type of device this is. DeviceTypes.h has the list
}

/*
 * Load the device configuration.
 * If possible values are read from EEPROM. If not, reasonable default values
 * are chosen and the configuration is overwritten in the EEPROM.
 */
void Example::loadConfiguration() 
{
    ExampleConfiguration *config = (ExampleConfiguration *) getConfiguration();

    if (!config) { // as lowest sub-class make sure we have a config object
        config = new ExampleConfiguration();
        Logger::debug("loading configuration in example device");
        setConfiguration(config);
    }
    
    Device::loadConfiguration(); // call parent

    //You can call your variables anything you like when loading/saving but obviously load and save with the same names
    //The name has no real size limit. Use names that are descriptive enough for you but short enough that you're willing
    //to type them every so often. ;)
    //If the user has not saved a custom value then the default value specified below will be used instead. So, set them 
    //to something sane. Note the use of & in front of the config variables here. This syntax of &config->yourVariable
    //is required. Basically just change the part after -> to point to all of your variables.

    //                  VarName      Var For Storage    Default value
    prefsHandler->read("FirstVal", &config->firstValue, 1);
    prefsHandler->read("SecondVal", &config->secondValue, 6000);
    prefsHandler->read("ThirdIsFloat", &config->fractionalValue, 4.5f);
}

/*
 * Store the current configuration to EEPROM
 */
void Example::saveConfiguration() 
{
    ExampleConfiguration *config = (ExampleConfiguration *) getConfiguration();

    Device::saveConfiguration(); // call parent
    //basically the same as above when loading but here there is no & in front of variables and no default value.
    prefsHandler->write("FirstVal", config->firstValue);
    prefsHandler->write("SecondVal", config->secondValue);
    prefsHandler->write("ThirdIsFloat", config->fractionalValue);

    prefsHandler->saveChecksum(); //not actually used right now but maybe some day?
    //this forces the in-memory copy to get written to EEPROM immediately. 
    //You don't have to but if you don't then a power cycle might zap your new values
    prefsHandler->forceCacheWrite(); 
}

//And, finally, instantiate a copy of the class. This will automatically register it
//with the rest of the system thus allowing you to enable/disable it and use it.
//it doesn't matter what your variable is named but obviously you can't have two global
//variables with the same name so keep that in mind and make it unique.
Example example;
