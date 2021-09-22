GEVCU7
======

Generalized Electric Vehicle Control Unit

Our website can be found at : http://www.evtv.me

A project to create a fairly Arduino compatible ECU firmware
to interface with various electric vehicle hardware over CANbus
(and possibly other comm channels)

The project now builds in the Arduino IDE. So, use it to compile, send the firmware to the GEVCU hardware, and monitor serial. It all works very nicely.

You will need the following to have any hope of compiling and running the firmware:
- A GEVCU board. This version supports GEVCU7 hardware (the one with Teessy MicroMod)
- Arduino IDE 1.6.10 - or later even 1.8.13 or such newer IDEs should work fine.
- TeensyDuino 1.54 (https://www.pjrc.com/teensy/td_download.html)
- All needed libraries come with TeensyDuino or are bundled in source

The canbus is supposed to be terminated on both ends of the bus. If you are testing with a DMOC and GEVCU then you've got two devices, each on opposing ends of the bus. GEVCU7 hardware is selectively terminated. By default it is not terminated but this can be solved by soldering the appropriate solder jumper


This software is MIT licensed:

Copyright (c) 2021 Collin Kidder, Michael Neuweiler, Charles Galpin, Jack Rickard

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

