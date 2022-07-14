/*
 * CrashHandler.h
 *
 * Monitors the crash reporter upon boot to see if we've crashed. If so try to log that
 * and figure out what to do. We may try to go back to a known good config or disable
 * all non-system devices just so it can boot. This would help people to not brick their
 * GEVCU with bad code.
 * 
 * This also handles setting breadcrumbs which can be picked up by the CrashReporter built
 * into the Teensy core. 
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


#ifndef CRASH_H_
#define CRASH_H_

#include <Arduino.h>
#include "config.h"
#include "Logger.h"
#include "MemCache.h"

extern MemCache *memCache;

#ifndef ENCODE_BREAD
#define ENCODE_BREAD(a) ( (((a[0]-0x40) & 0x1F) << 27) + (((a[1] - 0x40) & 0x1F) << 22) + (((a[2] - 0x40) & 0x1F) << 17) + (((a[3] - 0x40) & 0x1F) << 12) + (((a[4] - 0x40) & 0x1F) << 7) )
#endif

class CrashHandler {
public:
    CrashHandler(); //constructor
    void decodeBreadcrumbToSerial(uint32_t val);
    void decodeBreadcrumbToString(uint32_t val, char* buffer);
    void addBreadcrumb(uint32_t crumb);
    void updateBreadcrumb(uint8_t crumb);
    void analyzeCrashDataOnStartup();
    bool bCrashed();

private:
    struct crashreport_breadcrumbs_struct *bc = (struct crashreport_breadcrumbs_struct *)0x2027FFC0;
    uint32_t storedCrumbs[6]; //store the breadcrumbs from latest start up in RAM so we can keep accessing them whenever
    bool lastBootCrashed;
};

extern CrashHandler crashHandler;

#endif /* FAULT_H_ */


