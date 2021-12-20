/*
 * eeprom_layout.h
 *
EEPROM Map.

There is a 256KB eeprom chip which stores these settings. 
The 4K is allocated to primary storage and 4K is allocated to a "known good"
 storage location. 
This leaves most of EEPROM free for something else, probably logging.

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

#ifndef EEPROM_H_
#define EEPROM_H_

#include "config.h"

/*

Rethink this entire thing. For one, the devices should be setting their own EEPROM addresses
or otherwise not need to worry about the actual placement. Maybe store settings based on
a hash of the given variable name. A 32 bit hash shouldn't collide very often at all so
it may be possible to store settings that way. That adds 4 bytes of overhead but the EEPROM
is quite large so there's no reason it can't store enough data.

*/

/*
The device table is just a list of IDs. The devices register for a spot in the table.
Since each device has a 16 bit ID and the reserved space is 128 bytes we can support
64 different devices in the table and EEPROM
Devices are considered enabled if their highest ID bit is set (0x8000) otherwise
they're disabled.
This means that valid IDs must be under 0x8000 but that still leaves a couple of open IDs ;)
First device entry is 0xDEAD if valid - otherwise table is initialized
*/
#define EE_DEVICE_TABLE         512 //where is the table of devices found in EEPROM?

#define EE_DEVICE_SIZE          1024 //# of bytes allocated to each device
#define EE_DEVICES_BASE         1024 //start of where devices in the table can use
#define EE_SYSTEM_START         128

#define EE_MAIN_OFFSET          0 //offset from start of EEPROM where main config is
#define EE_LKG_OFFSET           34816  //start EEPROM addr where last known good config is

//start EEPROM addr where the system log starts. <SYS LOG YET TO BE DEFINED>
#define EE_SYS_LOG              69632

//start EEPROM addr for fault log (Used by fault_handler)
#define EE_FAULT_LOG            102400

/*Now, all devices also have a default list of things that WILL be stored in EEPROM. Each actual
implementation for a given device can store it's own custom info as well. This data must come after
the end of the stardard data. The below numbers are offsets from the device's eeprom section
*/

//first, things in common to all devices - leave 20 bytes for this
#define EE_CHECKSUM              0 //1 byte - checksum for this section of EEPROM to makesure it is valid
#define EE_DEVICE_ID             1 //2 bytes - the value of the ENUM DEVID of this device.

#define EEFAULT_VALID            0 //1 byte - Set to value of 0xB2 if fault data has been initialized
#define EEFAULT_READPTR          1 //2 bytes - index where reading should start (first unacknowledged fault)
#define EEFAULT_WRITEPTR         3 //2 bytes - index where writing should occur for new faults
#define EEFAULT_RUNTIME          5 //4 bytes - stores the number of seconds (in tenths) that the system has been turned on for - total time ever
#define EEFAULT_FAULTS_START     10 //a bunch of faults stored one after the other start at this location

#endif



