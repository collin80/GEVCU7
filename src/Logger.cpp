/*
 * Logger.cpp
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

/*
The astute might realize that the log level can be set from -1 to 3 not just 0-3.
-1 level is called avalanche and it is what it says. This is extra debugging.
You will get a LOT of traffic on the serial console if you do this.
*/

#include "Logger.h"
#include "SD.h"
#include "RingBuf.h"
#include "DeviceManager.h"
#include "devices/misc/SystemDevice.h"
#include "devices/esp32/ESP32Driver.h"

extern bool sdCardWorking;
FsFile logFile;

// For efficiency the log has to be preallocated for a certain amount of space.
//going to try not to do this. We don't need super high throughput for logging
//and the log file could grow arbitrarily large
#define LOG_FILE_SIZE 1000000  // 1M bytes.

#define RING_BUF_CAPACITY 16 * 1024
#define LOG_FILENAME "GevcuLog"
#define MAX_LOGFILES 200
#define CFG_TICK_INTERVAL_SDLOGGING 40000

// RingBuf for File type FsFile.
DMAMEM RingBuf<FsFile, RING_BUF_CAPACITY> rb;

uint32_t Logger::lastLogTime = 0;
ESP32Driver* Logger::esp32 = nullptr;

void Logger::initializeFile()
{
    char fn1[100];
    char fn2[100];
    //basically, rename all files one number higher than they were then start a new log with no numbers
    //as the current log
    for (int i = MAX_LOGFILES - 1; i > 0; i--)
    {
        snprintf(fn1, 200, "%s%d.txt", LOG_FILENAME, i);
        snprintf(fn2, 200, "%s%d.txt", LOG_FILENAME, i - 1);
        SD.sdfs.remove(fn1); //delete any file that may have been there
        SD.sdfs.rename(fn2, fn1); //rename the file to the name we just (maybe) deleted
    }
    snprintf(fn1, 200, "%s1.txt", LOG_FILENAME);
    snprintf(fn2, 200, "%s.txt", LOG_FILENAME);
    SD.sdfs.remove(fn1);
    SD.sdfs.rename(fn2, fn1);

    logFile = SD.sdfs.open(fn2, O_RDWR | O_CREAT | O_TRUNC);
    if (!logFile) {
        Serial.println("open failed\n");
        return;
    }
    else Serial.println("Log file has been opened for writing.");
    // File must be pre-allocated to avoid huge
    // delays searching for free clusters.
    /*
    if (!file.preAllocate(LOG_FILE_SIZE)) {
        Serial.println("preAllocate failed\n");
        file.close();
        return;
    }
    else Serial.println("File Pre_Alloc OK");
    */
    
    // initialize the RingBuf.
    rb.begin(&logFile);
    Serial.println("Initialized RingBuff");

    if (CrashReport) 
    {
        rb.println(CrashReport);
        flushFile();
    }
}

void Logger::flushFile()
{
    size_t n = rb.bytesUsed();
    int ret = 0;
    int writeBytes = min(n, 512u);
    ret = rb.writeOut(writeBytes);
    if (writeBytes != ret) {
        Serial.printf("Writeout failed. Want to write %u bytes but wrote %u\n", writeBytes, ret);
        logFile.close();
        return;
    }
    else logFile.flush(); //make sure it is updated on disk
}

//if there is a sector to write or 1 second has gone by then save the data
void Logger::loop()
{
    esp32 = static_cast<ESP32Driver *>(deviceManager.getDeviceByID(0x0800));
    static uint32_t lastWriteTime = 0;
    if (!sdCardWorking) return;
    size_t n = rb.bytesUsed();
    //Serial.println(n);
    //delay(100);
    if ( ( (n >= 512) || ((millis() - lastWriteTime) > 1000) ) && !logFile.isBusy()) {
      // Not busy only allows one sector before possible busy wait.
      // Write one sector from RingBuf to file.
      flushFile();
      lastWriteTime = millis();
    }
}

/*
 * Output a very verbose debugging message with a variable amount of parameters.
 * printf() style, see Logger::log()
 * 
 */
void Logger::avalanche(const char *message, ...) {
    if (sysConfig && (sysConfig->logLevel > Avalanche) )
        return;
    va_list args;
    va_start(args, message);
    Logger::log((DeviceId) NULL, Avalanche, message, args);
    va_end(args);
}

/*
 * Output a debug message with the name of a device appended before the message
 * printf() style, see Logger::log()
 */
void Logger::avalanche(DeviceId deviceId, const char *message, ...) {
    if (sysConfig && (sysConfig->logLevel > Avalanche) )
        return;
    va_list args;
    va_start(args, message);
    Logger::log(deviceId, Avalanche, message, args);
    va_end(args);
}

/*
 * Output a debug message with a variable amount of parameters.
 * printf() style, see Logger::log()
 *
 */
void Logger::debug(const char *message, ...) {
    if (sysConfig && (sysConfig->logLevel > Debug) )
        return;
    va_list args;
    va_start(args, message);
    Logger::log((DeviceId) NULL, Debug, message, args);
    va_end(args);
}

/*
 * Output a debug message with the name of a device appended before the message
 * printf() style, see Logger::log()
 */
void Logger::debug(DeviceId deviceId, const char *message, ...) {
    if (sysConfig && (sysConfig->logLevel > Debug) )
        return;
    va_list args;
    va_start(args, message);
    Logger::log(deviceId, Debug, message, args);
    va_end(args);
}

/*
 * Output a info message with a variable amount of parameters
 * printf() style, see Logger::log()
 */
void Logger::info(const char *message, ...) {
    if (sysConfig && (sysConfig->logLevel > Info) )
        return;
    va_list args;
    va_start(args, message);
    Logger::log((DeviceId) NULL, Info, message, args);
    va_end(args);
}

/*
 * Output a info message with the name of a device appended before the message
 * printf() style, see Logger::log()
 */
void Logger::info(DeviceId deviceId, const char *message, ...) {
    if (sysConfig && (sysConfig->logLevel > Info) )
        return;
    va_list args;
    va_start(args, message);
    Logger::log(deviceId, Info, message, args);
    va_end(args);
}

/*
 * Output a warning message with a variable amount of parameters
 * printf() style, see Logger::log()
 */
void Logger::warn(const char *message, ...) {
    if (sysConfig && (sysConfig->logLevel > Warn) )
        return;
    va_list args;
    va_start(args, message);
    Logger::log((DeviceId) NULL, Warn, message, args);
    va_end(args);
}

/*
 * Output a warning message with the name of a device appended before the message
 * printf() style, see Logger::log()
 */
void Logger::warn(DeviceId deviceId, const char *message, ...) {
    if (sysConfig && (sysConfig->logLevel > Warn) )
        return;
    va_list args;
    va_start(args, message);
    Logger::log(deviceId, Warn, message, args);
    va_end(args);
}

/*
 * Output a error message with a variable amount of parameters
 * printf() style, see Logger::log()
 */
void Logger::error(const char *message, ...) {
    if (sysConfig && (sysConfig->logLevel > Error) )
        return;
    va_list args;
    va_start(args, message);
    Logger::log((DeviceId) NULL, Error, message, args);
    va_end(args);
}

/*
 * Output a error message with the name of a device appended before the message
 * printf() style, see Logger::log()
 */
void Logger::error(DeviceId deviceId, const char *message, ...) {
    if (sysConfig && (sysConfig->logLevel > Error) )
        return;
    va_list args;
    va_start(args, message);
    Logger::log(deviceId, Error, message, args);
    va_end(args);
}

/*
 * Output a comnsole message with a variable amount of parameters
 * printf() style, see Logger::logMessage()
 */
void Logger::console(const char *message, ...) {
    va_list args;
    va_start(args, message);
    char buff[200];
    vsprintf(buff, message, args);
    Serial.println(buff);
    va_end(args);
}

/*
 * Set the log level. Any output below the specified log level will be omitted.
 */
void Logger::setLoglevel(LogLevel level) {
    if (!sysConfig) return;
    sysConfig->logLevel = (int)level;
}

/*
 * Retrieve the current log level.
 */
Logger::LogLevel Logger::getLogLevel() {
    if (!sysConfig) return Logger::Debug;
    return (Logger::LogLevel)(sysConfig->logLevel);
}

/*
 * Return a timestamp when the last log entry was made.
 */
uint32_t Logger::getLastLogTime() {
    return lastLogTime;
}

/*
 * Returns if debug log level is enabled. This can be used in time critical
 * situations to prevent unnecessary string concatenation (if the message won't
 * be logged in the end).
 *
 * Example:
 * if (Logger::isDebug()) {
 *    Logger::debug("current time: %d", millis());
 * }
 */
boolean Logger::isDebug() {
    if (!sysConfig) return true;
    return (sysConfig->logLevel == Debug);
}

/*
 * Output a log message (called by debug(), info(), warn(), error(), console())
 *
 * Supports printf() like syntax:
 *
 */
void Logger::log(DeviceId deviceId, LogLevel level, const char *format, va_list args) {
    lastLogTime = millis();
    uint32_t thisTime = micros();
    String outputString;// = String(lastLogTime) + " - ";

    switch (level) {
    case Off:
        return;
        break;
    case Avalanche:
        outputString += "~";
        break;
    case Debug:
        //Serial.print("D");
        outputString += "D";
        break;
    case Info:
        //Serial.print("I");
        outputString += "I";
        break;
    case Warn:
        //Serial.print("W");
        outputString += "W";
        break;
    case Error:
        //Serial.print("E");
        outputString += "E";
        break;
    }
    char buff[200];
    snprintf(buff, 200, "(%f) ", thisTime / 1000000.0f);
    outputString += buff;

    if (deviceId)
        outputString += printDeviceName(deviceId);
        //Serial.print(printDeviceName(deviceId));

    //outputString += logMessage(format, args);
    //Serial.println(outputString);
    vsnprintf(buff, 200, format, args);
    outputString += buff;
    //outputString += "\n";
    Serial.println(outputString);
    if (sdCardWorking) rb.println(outputString);
    if (esp32) esp32->sendLogString(outputString); //if ESP32 module is loaded then try to send output to telnet as well
}

/*
 * When the deviceId is specified when calling the logger, print the name
 * of the device after the log-level. This makes it easier to identify the
 * source of the logged message.
 */
String Logger::printDeviceName(DeviceId deviceId) {
    Device *dev = deviceManager.getDeviceByID(deviceId);
    if (!dev) return String(" ");
    String devString = String("[") + dev->getShortName() + String("] ");
    return devString;
}


