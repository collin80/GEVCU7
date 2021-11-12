/*
 * config.h
 *
 * Defines the components to be used in the GEVCU and allows the user to configure
 * static parameters.
 *
 * Note: Make sure with all pin defintions of your hardware that each pin number is
 *       only defined once.

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
 *      Author: Michael Neuweiler
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include <FlexCAN_T4.h>

#define CFG_BUILD_NUM	1070      //increment this every time a git commit is done. 
#define CFG_VERSION "GEVCU 2021-07-25"

#define portMEMORY_BARRIER()     __asm volatile ( "dmb" ::: "memory" )
#define portDATA_SYNC_BARRIER()  __asm volatile ( "dsb" ::: "memory" )
#define portINSTR_SYNC_BARRIER() __asm volatile ( "isb" )

/*
 * SERIAL CONFIGURATION
 */
//really does nothing. The Teensy processor actually could basically saturate the 480mbit USB connection if it wanted to.
#define CFG_SERIAL_SPEED 115200

//The defines that used to be here to configure devices are gone now.
//The EEPROM stores which devices to bring up at start up and all
//devices are programmed into the firware at the same time.

/*
 * ARRAY SIZE
 *
 * Define the maximum number of various object lists.
 * These values should normally not be changed.
 */
#define CFG_DEV_MGR_MAX_DEVICES 60 // the maximum number of devices supported by the DeviceManager
#define CFG_CAN_NUM_OBSERVERS	12 // maximum number of device subscriptions per CAN bus
#define CFG_TIMER_NUM_OBSERVERS	12 // the maximum number of supported observers per timer
#define CFG_TIMER_USE_QUEUING	// if defined, TickHandler uses a queuing buffer instead of direct calls from interrupts - MUCH safer!
#define CFG_TIMER_BUFFER_SIZE	100 // the size of the queuing buffer for TickHandler
#define CFG_FAULT_HISTORY_SIZE	50 //number of faults to store in eeprom. A circular buffer so the last 50 faults are always stored.

/*
 * PIN ASSIGNMENT
 */
#define CFG_THROTTLE_NONE	255
#define BLINK_LED           13 //13 is L, 73 is TX, 72 is RX
#define SD_DETECT           5

#define NUM_ANALOG	8
#define NUM_DIGITAL	12
#define NUM_OUTPUT	8
#define NUM_EXT_IO  24

//These allow the code to automatically configure up to 6 devices when the device table is initialized
//Set to 0xFFFF to not set a device. Device numbers used here are found in the respective headers for the devices
//or on the main configuration screen.
#define AUTO_ENABLE_DEV1    0x1000 //DMOC645
#define AUTO_ENABLE_DEV2    0x1031 //pot throttle
#define AUTO_ENABLE_DEV3    0xFFFF 
#define AUTO_ENABLE_DEV4    0xFFFF 
#define AUTO_ENABLE_DEV5    0xFFFF
#define AUTO_ENABLE_DEV6    0xFFFF

#endif /* CONFIG_H_ */


