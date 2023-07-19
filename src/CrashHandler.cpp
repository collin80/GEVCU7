/*
 * CrashHandler.cpp
 *
Copyright (c) 2022 Collin Kidder

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

#include "CrashHandler.h"
#include "eeprom_layout.h"

CrashHandler::CrashHandler()
{
    lastBootCrashed = false;
}

/*
This should be called early in code start up. It will analyze and display the crash dump
but also store the breadcrumbs which are otherwise immediately deleted. The rest of
the bc structure should not change during runtime so it's probably OK to leave that stuff as-is
*/
void CrashHandler::analyzeCrashDataOnStartup()
{
    if ( CrashReport )
    {
        Serial.println("SYSTEM CRASHED! Analyzing the crash data.");
        lastBootCrashed = true;
        if (bc->bitmask) {
            for (int i=0; i < 6; i++) {
                if (bc->bitmask & (1 << i)) {
                    Serial.print("Breadcrumb #");
                    Serial.print(i + 1);
                    Serial.print(" was ");
                    decodeBreadcrumbToSerial(bc->value[i]);
                    storedCrumbs[i] = bc->value[i];
                }
            }
        }
        Serial.println(CrashReport);
    }
    else
    {
        Serial.println("No prior crash detected, Good news!");
        for (int i = 0; i < 6; i++) storedCrumbs[i] = 0;
        lastBootCrashed = false;
    }
}

bool CrashHandler::bCrashed()
{
    return lastBootCrashed;
}

void CrashHandler::decodeBreadcrumbToSerial(uint32_t val)
{
  Serial.write( (val >> 27) + 0x40);
  Serial.write( ((val >> 22) & 0x1f) + 0x40);
  Serial.write( ((val >> 17) & 0x1f) + 0x40);
  Serial.write( ((val >> 12) & 0x1f) + 0x40);
  Serial.write( ((val >> 7) & 0x1f) + 0x40);
  Serial.println(val & 0x7F);
}

void CrashHandler::decodeBreadcrumbToString(uint32_t val, char* buffer)
{
  buffer[0] = (val >> 27) + 0x40;
  buffer[1] = ((val >> 22) & 0x1f) + 0x40;
  buffer[2] = ((val >> 17) & 0x1f) + 0x40;
  buffer[3] = ((val >> 12) & 0x1f) + 0x40;
  buffer[4] = ((val >> 7) & 0x1f) + 0x40;
  sprintf(&buffer[5], "%02lx", val & 0x7F);
}

//cycles all breadcrumbs one forward and adds this to the end. This allows the latest
//6 breadcrumbs to always be stored. This routine will take about 100 cycles to run
//so don't call it all of the time but it's OK to sprinkle it around in areas where
//crashes are suspected to happen. Note that the processor runs at 600MHz so 100
//cycles is 1/6th of a microsecond. If you add 6 calls to your function you will
//cause a phantom 1us delay which you will have to be OK with. This overhead likely
//less than using serial writes to the USB port though.
//Breadcrumbs are 32 bit and there are 6 of them.
void CrashHandler::addBreadcrumb(uint32_t crumb)
{
    //these are pipelined and cached and almost instant.
    bc->value[0] = bc->value[1];
    bc->value[1] = bc->value[2];
    bc->value[2] = bc->value[3];
    bc->value[3] = bc->value[4];
    bc->value[4] = bc->value[5];
    bc->value[5] = crumb;
    //have to flush to RAM or it will stay in cache. Here is where our delay really triggers
    arm_dcache_flush((void *)bc, sizeof(struct crashreport_breadcrumbs_struct));
}

//If you've already dropped a breadcrumb and just want to update the 7 bit number on the end
//then use this function instead. This allows for placing milestones at the end of the breadcrumb
//to see how far into a function it got. Note, however, that you can only do this if you have not
//called any other functions that might drop their own breadcrumb. This makes the utility
//somewhat limited but might be useful when debugging to annotate progress.
//note that this still calls dcache flush and so still causes a healthy (1/6 of 1uS) processing delay
void CrashHandler::updateBreadcrumb(uint8_t crumb)
{
    bc->value[5] = (bc->value[5] & 0xFFFFFFF8) + (crumb & 0x7F);
    //have to flush to RAM or it will stay in cache
    arm_dcache_flush((void *)bc, sizeof(struct crashreport_breadcrumbs_struct));
}

CrashHandler crashHandler;


