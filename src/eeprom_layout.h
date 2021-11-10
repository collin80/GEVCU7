/*
 * eeprom_layout.h
 *
EEPROM Map. There is support for up to 6 devices: 
A motor controller, display, charger, BMS, Throttle, 
and a misc device (EPAS, WOC, etc)

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
The device table is just a list of IDs. The devices register for a spot in the table.
Since each device has a 16 bit ID and the reserved space is 128 bytes we can support
64 different devices in the table and EEPROM
Devices are considered enabled if their highest ID bit is set (0x8000) otherwise
they're disabled.
This means that valid IDs must be under 0x8000 but that still leaves a couple of open IDs ;)
First device entry is 0xDEAD if valid - otherwise table is initialized
*/
#define EE_DEVICE_TABLE         512 //where is the table of devices found in EEPROM?

#define EE_DEVICE_SIZE          512 //# of bytes allocated to each device
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

//Motor controller data
#define EEMC_MAX_RPM             20 //2 bytes, unsigned int for maximum allowable RPM
#define EEMC_MAX_TORQUE          22 //2 bytes, unsigned int - maximum torque in tenths of a Nm
#define EEMC_PRECHARGE_RELAY     24 //1 byte - 255 = no precharge relay 0-3 = yes, there is one (and the output is the number stored)
#define EEMC_CONTACTOR_RELAY     25 //1 byte - 255 = no contactor relay 0-3 = yes there is
#define EEMC_COOL_FAN            26 //1 byte output controlling external cooling relay
#define EEMC_COOL_ON             27 //1 bytes temperature at which external cooling is switched on
#define EEMC_COOL_OFF            28 //1 byte temperature at which external cooling is switched off
#define EEMC_KILOWATTHRS         29 //4 bytes - capacitance of controller capacitor bank in micro farads (uf) - set to zero to disable RC precharge
#define EEMC_PRECHARGE_R         33 //2 bytes - Resistance of precharge resistor in tenths of an ohm
#define EEMC_NOMINAL_V           35 //2 bytes - nominal system voltage to expect (in tenths of a volt)
#define EEMC_REVERSE_LIMIT       37 //2 bytes - a percentage to knock the requested torque down by while in reverse.
#define EEMC_RPM_SLEW_RATE       39 //2 bytes - slew rate (rpm/sec) at which speed should change (only in speed mode)
#define EEMC_TORQUE_SLEW_RATE    41 //2 bytes - slew rate (0.1Nm/sec) at which the torque should change
#define EEMC_BRAKE_LIGHT         42
#define EEMC_REV_LIGHT           43
#define EEMC_ENABLE_IN           44
#define EEMC_REVERSE_IN          45
#define EEMC_MOTOR_MODE          46
#define EEMC_TAPER_UPPER         50 //2 bytes
#define EEMC_TAPER_LOWER         52 //2 bytes

//throttle data
#define EETH_MIN_ONE             20 //2 bytes - ADC value of minimum value for first channel
#define EETH_MAX_ONE             22 //2 bytes - ADC value of maximum value for first channel
#define EETH_MIN_TWO             24 //2 bytes - ADC value of minimum value for second channel
#define EETH_MAX_TWO             26 //2 bytes - ADC value of maximum value for second channel
#define EETH_REGEN_MIN           28 //2 bytes - unsigned int - tenths of a percent (0-1000) of pedal position where regen stops
#define EETH_FWD                 30 //2 bytes - unsigned int - tenths of a percent (0-1000) of pedal position where forward motion starts 
#define EETH_MAP                 32 //2 bytes - unsigned int - tenths of a percent (0-1000) of pedal position where forward motion is at 50% throttle
#define EETH_BRAKE_MIN           34 //2 bytes - ADC value of minimum value for brake input
#define EETH_BRAKE_MAX           36 //2 bytes - ADC value of max value for brake input
#define EETH_MAX_ACCEL_REGEN     38 //2 bytes - maximum percentage of throttle to command on accel pedal regen
#define EETH_MAX_BRAKE_REGEN     40 //2 bytes - maximum percentage of throttle to command for braking regen. Starts at min brake regen and works up to here.
#define EETH_NUM_THROTTLES       42 //1 byte - How many throttle inputs should we use? (1 or 2)
#define EETH_THROTTLE_TYPE       43 //1 byte - Allow for different throttle types. For now 1 = Linear pots, 2 = Inverse relationship between pots. See Throttle.h
#define EETH_MIN_BRAKE_REGEN     44 //2 bytes - the starting level for brake regen as a percentage of throttle
#define EETH_MIN_ACCEL_REGEN     46 //2 bytes - the starting level for accelerator regen as a percentage of throttle
#define EETH_REGEN_MAX           48 //2 bytes - unsigned int - tenths of a percent (0-1000) of pedal position where regen is at maximum
#define EETH_CREEP               50 //2 bytes - percentage of throttle used to simulate creep
#define EETH_CAR_TYPE            52 //1 byte - type of car for querying the throttle position via CAN bus
#define EETH_ADC_1               53 //1 byte - which ADC port to use for first throttle input
#define EETH_ADC_2               54 //1 byte - which ADC port to use for second throttle input

//System Data
#define EESYS_LOG_LEVEL          5   //1 byte - the log level
#define EESYS_SYSTEM_TYPE        10  //1 byte - 1 = Old school protoboards 2 = GEVCU2/DUED 3 = GEVCU3, 4 = GEVCU4 or 5, 6 = GEVCU6 - Defaults to 2 if invalid or not set up
#define EESYS_ADC0_GAIN          30  //2 bytes - ADC gain centered at 1024 being 1 to 1 gain, thus 512 is 0.5 gain, 2048 is double, etc
#define EESYS_ADC0_OFFSET        32  //2 bytes - ADC offset from zero - ADC reads 12 bit so the offset will be [0,4095] - Offset is subtracted from read ADC value
#define EESYS_ADC1_GAIN          34  //2 bytes - ADC gain centered at 1024 being 1 to 1 gain, thus 512 is 0.5 gain, 2048 is double, etc
#define EESYS_ADC1_OFFSET        36  //2 bytes - ADC offset from zero - ADC reads 12 bit so the offset will be [0,4095] - Offset is subtracted from read ADC value
#define EESYS_ADC2_GAIN          38  //2 bytes - ADC gain centered at 1024 being 1 to 1 gain, thus 512 is 0.5 gain, 2048 is double, etc
#define EESYS_ADC2_OFFSET        40  //2 bytes - ADC offset from zero - ADC reads 12 bit so the offset will be [0,4095] - Offset is subtracted from read ADC value
#define EESYS_ADC3_GAIN          42  //2 bytes - ADC gain centered at 1024 being 1 to 1 gain, thus 512 is 0.5 gain, 2048 is double, etc
#define EESYS_ADC3_OFFSET        44  //2 bytes - ADC offset from zero - ADC reads 12 bit so the offset will be [0,4095] - Offset is subtracted from read ADC value
#define EESYS_ADC4_GAIN          46  //2 bytes - ADC gain centered at 1024 being 1 to 1 gain, thus 512 is 0.5 gain, 2048 is double, etc
#define EESYS_ADC4_OFFSET        48  //2 bytes - ADC offset from zero - ADC reads 12 bit so the offset will be [0,4095] - Offset is subtracted from read ADC value
#define EESYS_ADC5_GAIN          50  //2 bytes - ADC gain centered at 1024 being 1 to 1 gain, thus 512 is 0.5 gain, 2048 is double, etc
#define EESYS_ADC5_OFFSET        52  //2 bytes - ADC offset from zero - ADC reads 12 bit so the offset will be [0,4095] - Offset is subtracted from read ADC value
#define EESYS_ADC6_GAIN          54  //2 bytes - ADC gain centered at 1024 being 1 to 1 gain, thus 512 is 0.5 gain, 2048 is double, etc
#define EESYS_ADC6_OFFSET        56  //2 bytes - ADC offset from zero - ADC reads 12 bit so the offset will be [0,4095] - Offset is subtracted from read ADC value
#define EESYS_ADC7_GAIN          58  //2 bytes - ADC gain centered at 1024 being 1 to 1 gain, thus 512 is 0.5 gain, 2048 is double, etc
#define EESYS_ADC7_OFFSET        60  //2 bytes - ADC offset from zero - ADC reads 12 bit so the offset will be [0,4095] - Offset is subtracted from read ADC value
#define EESYS_CAN0_BAUD          80  //2 bytes - Baud rate of CAN0 in 1000's of baud. So a value of 500 = 500k baud. Set to 0 to disable CAN0
#define EESYS_CAN1_BAUD          82  //2 bytes - Baud rate of CAN1 in 1000's of baud. So a value of 500 = 500k baud. Set to 0 to disable CAN1
#define EESYS_CAN2_BAUD          84  //2 bytes - baud rate of CAN2 in 1k baud. 500=500,000. Set to 0 to disable
#define EESYS_SWCAN_BAUD         86  //2 bytes - baud rate of SWCAN in 3 baud increments. 33.333k is thus 11,111 and 100k is 33333. Set to 0 to disable

#define EEBMS_CAPACITY           20 //2 bytes - pack capacity in 1/10 amp hours
#define EEBMS_AH                 22 //4 bytes - Current number of AH remaining in pack (in millionths of an AH)
#define EEBMS_HI_VOLT_LIM        26 //2 bytes - Voltage above which the BMS should disallow charging - Tenths of a V
#define EEBMS_LO_VOLT_LIM        28 //2 bytes - Voltage below which the BMS show disallow any discharge - Tenths of a V
#define EEBMS_HI_CELL_LIM        30 //2 bytes - Cell voltage above which a fault is raised - In hundredths of a volt
#define EEBMS_LO_CELL_LIM        32 //2 bytes - Cell voltage under which a fault is raised - In hundredths of a volt
#define EEBMS_HI_TEMP_LIM        34 //2 bytes - Cell temperature over which a fault is raised - In tenths of a degree C
#define EEBMS_LO_TEMP_LIM        36 //2 bytes - Cell temperature under which a fault is raised - Tenths of a degree C

#define EEFAULT_VALID            0 //1 byte - Set to value of 0xB2 if fault data has been initialized
#define EEFAULT_READPTR          1 //2 bytes - index where reading should start (first unacknowledged fault)
#define EEFAULT_WRITEPTR         3 //2 bytes - index where writing should occur for new faults
#define EEFAULT_RUNTIME          5 //4 bytes - stores the number of seconds (in tenths) that the system has been turned on for - total time ever
#define EEFAULT_FAULTS_START     10 //a bunch of faults stored one after the other start at this location


#endif



