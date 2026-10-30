GEVCU7
======

Generalized Electric Vehicle Control Unit

Our website can be found at : http://www.evtv.me

A project to create a fairly Arduino compatible ECU firmware
to interface with various electric vehicle hardware over CANbus
(and possibly other comm channels)

The project will really only compile properly with PlatformIO
due to the advanced directory layout.

You will need the following to have any hope of compiling and running the firmware:
- A GEVCU board. This version supports GEVCU7 hardware (the one with Teessy MicroMod)
- PlatformIO (Works well in Visual Studio Code)
- TeensyDuino (1.58 or higher) but PlatformIO will download and set this up automatically.
- All libraries are bundled with TeensyDuino or included in the lib folder

Follow these basic steps to get up and running:
1. Download the code in this repo, either through git or by downloading a zip
2. Download and install Visual Studio Code
3. Go to the extensions tab in VSC and type in "PlatformIO". The extensions tab looks like four blocks where one is pulled away from the other threee (Also found in View menu). Install "PlatformIO IDE" from the verified PlatformIO author
4. Once PlatformIO has fully installed it will want you to restart Visual Studio Code. Do so.
5. Go the File menu and select "Open Folder". Open the folder where your GEVCU7 files are installed. This will cause the PlatformIO extension to find the platformio.ini file and begin to download the needed Teensy support files (Teensyduino). This will take some time. When it is finished there will be new PlatformIO controls at the bottom of the VSC window.
6. Down there is a new icon that looks like a checkmark. This will let you compile GEVCU7. The Arrow icon next to it will cause it to compile and upload GEVCU7. 

Note that a change committed in October of 2026 has transitioned the
code to using TeensyThreads which causes a variety of code to run in 
separate preemptively multitasked threads. Care should be taken when 
passing data between threads. The multithreading is still being worked on.

Threading and mutex rules
-------------------------

Every device's handleTick() runs in its own thread. CAN frames are dispatched
from the CANProc thread, the SD log is written from the SDCard thread and
console commands run from loop() (thread 0). Anything shared between these
must be protected.

Use RecursiveMutex / RecursiveScope (src/RecursiveMutex.h), not
Threads::Mutex directly. The TeensyThreads mutex does not track its owner so a
thread that locks one it already holds waits on itself forever.

When more than one lock is needed they must ALWAYS be taken in this order
(a lock may only be taken while holding locks that appear above it):

1. canbusMutex     (CanHandler.cpp)  - CAN hardware. Held while CAN frames are dispatched to observers
2. sdAccessMutex   (GEVCU7.cpp)      - anything touching the SD card
3. faultMutex      (FaultHandler)    - fault list and pointers
4. cacheMutex      (MemCache)        - EEPROM page cache
5. i2cMutex        (GEVCU7.cpp)      - I2C bus, PCA output cache and PWM state in SystemIO
6. tickMutex (TickHandler), statusMutex (DeviceManager), jsonMutex (ESP32Driver),
   adcMutex (SystemIO) - none of these may take any lock above while held
7. logMutex        (Logger.cpp)      - Logger::log / Logger::console output
8. txMutex         (ESP32Driver)     - writes to Serial2 (the ESP32). Don't log while holding it
9. consoleMutex    (SerialConsole)   - console line queue. Nothing may be locked while holding it

Logging is allowed while holding any lock above it. Serial writes can call
yield() which can run serialEvent() / serialEvent2() in whatever thread is
writing, which is why the console queue sits below the logger.

Other rules:
- Code that runs from yield() (serialEvent, serialEvent2 and anything they call)
  may only take consoleMutex. Queue the work and do it elsewhere, as
  SerialConsole does with runQueuedCommands() and ESP32Driver does with
  processQueuedRequests() (both run from loop()).
- Don't call into other devices or observers while holding a lock. Copy what
  you need, release the lock, then call (see DeviceManager::handleTick).
- Hand out copies, not pointers into shared data (see FaultHandler::getFault
  and DeviceManager::getStatusEntryByHash).
- Never allocate memory from an interrupt handler. Heap use from threads is
  made safe by src/MallocLock.cpp but that does not cover ISRs.
- A lock held across threads.delay() blocks every other thread that wants it.
  MemCache does this on purpose while an EEPROM page write finishes.

The canbus is supposed to be terminated on both ends of the bus. 
If you are testing with a DMOC and GEVCU then you've got two devices, 
each on opposing ends of the bus. GEVCU7 hardware is selectively terminated.
By default it is not terminated but this can be solved by soldering the 
appropriate solder jumper.


This software is MIT licensed:

Copyright (c) 2021-2026 Collin Kidder, Michael Neuweiler, Charles Galpin, Jack Rickard

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

