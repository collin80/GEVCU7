/*
 * GEVCU.h
 *
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

#ifndef GEVCU_H_
#define GEVCU_H_

#include <Arduino.h>
#include "src/config.h"
#include "src/devices/Device.h"
#include "src/devices/io/Throttle.h"
#include "src/devices/bms/BatteryManager.h"
#include "src/devices/motorctrl/MotorController.h"
#include "src/Heartbeat.h"
#include "src/sys_io.h"
#include "src/CanHandler.h"
#include "src/MemCache.h"
#include "src/devices/io/ThrottleDetector.h"
#include "src/DeviceManager.h"
#include "src/SerialConsole.h"
#include "src/devices/display/ELM327_Emu.h"
#include "src/Sys_Messages.h"
#include "src/FaultHandler.h"
#include "src/devices/dcdc/DCDCController.h"

#ifdef __cplusplus
extern "C" {
#endif
void loop();
void setup();

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* GEVCU_H_ */


