//******************************************************************************
// FlasherX -- firmware "OTA" update via Intel Hex file over SDCard file
//             This has been modified for two reasons:
//             1: To make it load the hex file from the sdcard instead of serial
//             2: To make it work directly inside of the GEVCU source code.
//******************************************************************************
//
// WARNING: You can brick your Teensy with incorrect flash erase/write, such as
// incorrect flash config (0x400-40F). This code may or may not prevent that.
//
// Based on Flasher3 (Teensy 3.x) and Flasher4 (Teensy 4.x) by Jon Zeeff
//
// Jon Zeeff 2016, 2019, 2020
// This code is in the public domain.  Please retain my name and
// in distributed copies, and let me know about any bugs
//
// I, Jon Zeeff, give no warranty, expressed or implied for this software
// and/or documentation provided, including, without limitation, warranty of
// merchantability and fitness for a particular purpose.

// 7th modication - Switch to SDCard file, slight mods to work with GEVCU (Collin Kidder)
// 6th Modifications 11/18/21 bug fix in file FlashTXX.cpp by Joe Pasquariello
//   - fix logic in while loop in flash_block_write() in FlashTXX
// 5th Modifications 10/27/21 add support for Teensy Micromod by Joe Pasquariello
//   - define macros for TEENSY_MICROMOD w/ same values as for TEENSY40
//   - update FLASH_SIZE for T4.1 and TMM from 2MB to 8MB
// 4th Modifications merge of Flasher3/4 and new features by Joe Pasquariello
//   - FLASH buffer dynamically sized from top of existing code to FLASH_RESERVE
//   - optional RAM buffer option for T4.x via macro RAM_BUFFER_SIZE > 0
//   - Stream* (USB or UART) and buffer addr/size set at run-time
//   - incorporate Frank Boesing's FlashKinetis routines for T3.x
//   - add support for Teensy 4.1 and Teensy LC
//    This code is released into the public domain.
// 3rd Modifications for T3.5 and T3.6 in Dec 2020 by Joe Pasquariello
//    This code is released into the public domain.
// 2nd Modifications for teensy 3.5/3/6 by Deb Hollenback at GiftCoder
//    This code is released into the public domain.
//    see https://forum.pjrc.com/threads/43165-Over-the-Air-firmware-updates-changes-for-flashing-Teensy-3-5-amp-3-6
// 1st Modifications by Jon Zeeff
//    see https://forum.pjrc.com/threads/29607-Over-the-air-updates
// Original by Niels A. Moseley, 2015.
//    This code is released into the public domain.
//    https://namoseley.wordpress.com/2015/02/04/freescale-kinetis-mk20dx-series-flash-erasing/

#include <Arduino.h>
#include <stdio.h>		// sscanf(), etc.
#include <string.h>		// strlen(), etc.
#include "FlashTxx.h"		// TLC/T3x/T4x/TMM flash primitives
#include "FlasherX.h"
#include <Watchdog_t4.h>
#include <TeensyThreads.h>
#include "config.h"
#include <Logger.h>

extern WDT_T4<WDT3> wdt;

const int ledPin = 13;		// LED
extern Stream *serialptr;

void setup_flasherx() 
{
  Serial.printf( "\nFlasherX OTA/SDCard firmware update v1 %s %s\n", __DATE__, __TIME__ );
  Serial.printf( "target = %s (%dK flash in %dK sectors)\n",
			FLASH_ID, FLASH_SIZE/1024, FLASH_SECTOR_SIZE/1024);

  pinMode(ledPin, OUTPUT);	// assign output
  digitalWrite(ledPin, HIGH);	// set the LED on
  threads.delay(200);			// delay
  digitalWrite(ledPin, LOW);	// set the LED off
}

void start_upgrade(FsFile *file)
{

  uint32_t buffer_addr, buffer_size;

  if (firmware_buffer_init( &buffer_addr, &buffer_size ) == 0)
  {
    Logger::error("unable to create buffer" );
    return;
  }

  Logger::info( "buffer = %1luK %s (%08lX - %08lX)",
		buffer_size/1024, IN_FLASH(buffer_addr) ? "FLASH" : "RAM",
		buffer_addr, buffer_addr + buffer_size );

  // receive hex file via sdcard file, write new firmware to flash, clean up, reboot
  update_firmware( file, buffer_addr, buffer_size ); // no return if success
  
  // return from update_firmware() means error or user abort, so clean up and
  // reboot to ensure that static vars get boot-up initialized before retry
  Serial.printf( "erase FLASH buffer / free RAM buffer...\n" );
  firmware_buffer_free( buffer_addr, buffer_size );
  Serial.flush();
  REBOOT;
}

//******************************************************************************
// update_firmware()	read hex file and write new firmware to program flash
//******************************************************************************
void update_firmware( FsFile *file, uint32_t buffer_addr, uint32_t buffer_size )
{
    static char line[96];					// buffer for hex lines
    static char data[32] __attribute__ ((aligned (8)));	// buffer for hex data
    hex_info_t hex = {					// intel hex info struct
        data, 0, 0, 0,					//   data,addr,num,code
        0, 0xFFFFFFFF, 0, 					//   base,min,max,
        0, 0						//   eof,lines
    };

    Serial.printf( "waiting for hex lines...\n" );

    int dots = 0;
    int linecount = 0;
    bool blinkOn = false;

    digitalWrite(BLINK_LED, LOW);

    // read and process intel hex lines until EOF or error
    while (!hex.eof)
    {
        if (file) read_ascii_line_file( file, line, sizeof(line) );
        else
        {
          serialptr->write((char)0x97); //signal we want to receive a line
          read_ascii_line_serial( line, sizeof(line) );
        }
        wdt.feed();

        linecount++;
        if (linecount == 200)
        {
            linecount = 0;
            Serial.write('.');
            dots++;
            blinkOn = !blinkOn;
            digitalWrite(BLINK_LED, blinkOn);
            if (dots == 40)
            {
                dots = 0;
                Serial.write('\n');
            }
        }
        //Serial.printf("(%s)\n", line);

        if (strlen(line) > 2)
        {
            if (parse_hex_line( (const char*)line, hex.data, &hex.addr, &hex.num, &hex.code ) == 0)
            {
                Logger::error( "abort - bad hex line: \"%s\"", line );
                return;
            }
            else if (process_hex_record( &hex ) != 0)
            { // error on bad hex code
                Logger::error( "abort - invalid hex code %d", hex.code );
                return;
            }
            else if (hex.code == 0)
            { // if data record
                uint32_t addr = buffer_addr + hex.base + hex.addr - FLASH_BASE_ADDR;
                if (hex.max > (FLASH_BASE_ADDR + buffer_size))
                {
                    Logger::error( "abort - max address %08lX too large", hex.max );
                    return;
                }
                else if (!IN_FLASH(buffer_addr))
                {
                    memcpy( (void*)addr, (void*)hex.data, hex.num );
                }
                else if (IN_FLASH(buffer_addr))
                {
                    int error = flash_write_block( addr, hex.data, hex.num );
                    if (error)
                    {
                        Logger::error( "abort - error %02X in flash_write_block()", error );
	                      return;
                    }
                }
            }
            hex.lines++;
        }
    }
    
    Logger::info( "\nhex file: %1d lines, %1lu bytes, addresses (%08lX - %08lX)",
			  hex.lines, hex.max-hex.min, hex.min, hex.max );

    // check FSEC value in new code -- abort if incorrect
#if defined(KINETISK) || defined(KINETISL)
    uint32_t value = *(uint32_t *)(0x40C + buffer_addr);
    if (value == 0xfffff9de)
    {
        Serial.printf( "new code contains correct FSEC value %08lX\n", value );
    }
    else
    {
        Serial.printf( "abort - FSEC value %08lX should be FFFFF9DE\n", value );
        return;
    } 
#endif

    // check FLASH_ID in new code - abort if not found
    if (check_flash_id( buffer_addr, hex.max - hex.min ))
    {
        Logger::info( "new code contains correct target ID %s", FLASH_ID );
    }
    else
    {
        Logger::error( "abort - new code missing string %s", FLASH_ID );
        return;
    }
  
    //if we got this far then delete the file we had loaded

    if (file)
    {
        char filename[40];
        file->getName(filename, 40);
        file->close();
        SD.sdfs.remove(filename);
    }
  
    wdt.feed();

    Logger::info("About to write new firmware image");
    Logger::flushFile(); //force the write as we're about to go bye bye

    // move new program from buffer to flash, free buffer, and reboot
    flash_move( FLASH_BASE_ADDR, buffer_addr, hex.max-hex.min );

    // should not return from flash_move(), but put REBOOT here as reminder
    REBOOT;
}
