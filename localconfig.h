//This file is for local changes that may not be good to merge into the main code.
//The main point is to commit this to github once to give a template then never update it.
//This allows for setting these things locally without these settings then getting merged at github
//and messing someone up. Don't commit changes to this file to github unless something was added. Then be careful
//to have reasonable settings before the upload.

//These can be editted to get into the program faster
//but changing them too low isn't likely to be of much use
#define TEENSY_INIT_USB_DELAY_BEFORE 120
#define TEENSY_INIT_USB_DELAY_AFTER 200

//if this is defined there is a large start up delay so you can see the start up messages. NOT for production!
//#define DEBUG_STARTUP_DELAY

//If this is defined then we will ignore the hardware sd inserted signal and just claim it's inserted. Required for first prototype.
//Probably don't enable this on more recent hardware. Starting at the 2nd prototype the sdcard can properly be detected.
//#define ASSUME_SDCARD_INSERTED