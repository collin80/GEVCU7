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
- TeensyDuino (1.58 or higher) (https://www.pjrc.com/teensy/td_download.html)
- All libraries are bundled with TeensyDuino or included in the lib folder

Follow these basic steps to get up and running:
1. Download the code in this repo, either through git or by downloading a zip
2. Download and install Visual Studio Code
3. Go to the extensions tab in VSC and type in "PlatformIO". Install "PlatformIO IDE" from the verified PlatformIO author
4. Once PlatformIO has fully installed it will want you to restart Visual Studio Code. Do so.
5. Go the File menu and select "Open Folder". Open the folder where your GEVCU7 files are installed. This will cause the PlatformIO extension to find the platformio.ini file and begin to download the needed Teensy support files (Teensyduino). This will take some time. When it is finished there will be new PlatformIO controls at the bottom of the VSC window.
6. Down there is a new icon that looks like a checkmark. This will let you compile GEVCU7. The Arrow icon next to it will cause it to compile and upload GEVCU7. 

The canbus is supposed to be terminated on both ends of the bus. 
If you are testing with a DMOC and GEVCU then you've got two devices, 
each on opposing ends of the bus. GEVCU7 hardware is selectively terminated.
By default it is not terminated but this can be solved by soldering the 
appropriate solder jumper.


This software is MIT licensed:

Copyright (c) 2021-2024 Collin Kidder, Michael Neuweiler, Charles Galpin, Jack Rickard

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

