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

#include "Logger.h"

Logger::LogLevel Logger::logLevel = Logger::Info;
uint32_t Logger::lastLogTime = 0;

/*
 * Output a debug message with a variable amount of parameters.
 * printf() style, see Logger::log()
 *
 */
void Logger::debug(const char *message, ...) {
    if (logLevel > Debug)
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
    if (logLevel > Debug)
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
    if (logLevel > Info)
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
    if (logLevel > Info)
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
    if (logLevel > Warn)
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
    if (logLevel > Warn)
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
    if (logLevel > Error)
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
    if (logLevel > Error)
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
    Logger::logMessage(message, args);
    va_end(args);
}

/*
 * Set the log level. Any output below the specified log level will be omitted.
 */
void Logger::setLoglevel(LogLevel level) {
    logLevel = level;
}

/*
 * Retrieve the current log level.
 */
Logger::LogLevel Logger::getLogLevel() {
    return logLevel;
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
    return logLevel == Debug;
}

/*
 * Output a log message (called by debug(), info(), warn(), error(), console())
 *
 * Supports printf() like syntax:
 *
 * %% - outputs a '%' character
 * %s - prints the next parameter as string
 * %d - prints the next parameter as decimal
 * %f - prints the next parameter as double float
 * %x - prints the next parameter as hex value
 * %X - prints the next parameter as hex value with '0x' added before
 * %b - prints the next parameter as binary value
 * %B - prints the next parameter as binary value with '0b' added before
 * %l - prints the next parameter as long
 * %c - prints the next parameter as a character
 * %t - prints the next parameter as boolean ('T' or 'F')
 * %T - prints the next parameter as boolean ('true' or 'false')
 */
void Logger::log(DeviceId deviceId, LogLevel level, const char *format, va_list args) {
    lastLogTime = millis();
    Serial.print(lastLogTime);
    Serial.print(" - ");

    switch (level) {
    case Debug:
        Serial.print("DEBUG");
        break;
    case Info:
        Serial.print("INFO");
        break;
    case Warn:
        Serial.print("WARNING");
        break;
    case Error:
        Serial.print("ERROR");
        break;
    }
    Serial.print(": ");

    if (deviceId)
        printDeviceName(deviceId);

    logMessage(format, args);
}

/*
 * Output a log message (called by log(), console())
 *
 * Supports printf() like syntax:
 *
 * %% - outputs a '%' character
 * %s - prints the next parameter as string
 * %d - prints the next parameter as decimal
 * %f - prints the next parameter as double float
 * %x - prints the next parameter as hex value
 * %X - prints the next parameter as hex value with '0x' added before
 * %b - prints the next parameter as binary value
 * %B - prints the next parameter as binary value with '0b' added before
 * %l - prints the next parameter as long
 * %c - prints the next parameter as a character
 * %t - prints the next parameter as boolean ('T' or 'F')
 * %T - prints the next parameter as boolean ('true' or 'false')
 */
void Logger::logMessage(const char *format, va_list args) {
    for (; *format != 0; ++format) {
        if (*format == '%') {
            ++format;
            if (*format == '\0')
                break;
            if (*format == '%') {
                Serial.print(*format);
                continue;
            }
            if (*format == 's') {
                register char *s = (char *) va_arg( args, int );
                Serial.print(s);
                continue;
            }
            if (*format == 'd' || *format == 'i') {
                Serial.print(va_arg( args, int ), DEC);
                continue;
            }
            if (*format == 'f') {
                Serial.print(va_arg( args, double ), 2);
                continue;
            }
            if (*format == 'x') {
                Serial.print(va_arg( args, int ), HEX);
                continue;
            }
            if (*format == 'X') {
                Serial.print("0x");
                Serial.print(va_arg( args, int ), HEX);
                continue;
            }
            if (*format == 'b') {
                Serial.print(va_arg( args, int ), BIN);
                continue;
            }
            if (*format == 'B') {
                Serial.print("0b");
                Serial.print(va_arg( args, int ), BIN);
                continue;
            }
            if (*format == 'l') {
                Serial.print(va_arg( args, long ), DEC);
                continue;
            }

            if (*format == 'c') {
                Serial.print(va_arg( args, int ));
                continue;
            }
            if (*format == 't') {
                if (va_arg( args, int ) == 1) {
                    Serial.print("T");
                } else {
                    Serial.print("F");
                }
                continue;
            }
            if (*format == 'T') {
                if (va_arg( args, int ) == 1) {
                    Serial.print(Constants::trueStr);
                } else {
                    Serial.print(Constants::falseStr);
                }
                continue;
            }

        }
        Serial.print(*format);
    }
    Serial.println();
}

/*
 * When the deviceId is specified when calling the logger, print the name
 * of the device after the log-level. This makes it easier to identify the
 * source of the logged message.
 * NOTE: Should be kept in synch with the defined devices.
 */
void Logger::printDeviceName(DeviceId deviceId) {
    switch (deviceId) {
    case DMOC645:
        Serial.print("DMOC645");
        break;
    case BRUSA_DMC5:
        Serial.print("DMC5");
        break;
    case BRUSACHARGE:
        Serial.print("NLG5");
        break;
    case TCCHCHARGE:
        Serial.print("TCCH");
        break;
    case THROTTLE:
        Serial.print("THROTTLE");
        break;
    case POTACCELPEDAL:
        Serial.print("POTACCEL");
        break;
    case POTBRAKEPEDAL:
        Serial.print("POTBRAKE");
        break;
    case CANACCELPEDAL:
        Serial.print("CANACCEL");
        break;
    case CANBRAKEPEDAL:
        Serial.print("CANBRAKE");
        break;
    case ICHIP2128:
        Serial.print("ICHIP");
        break;
    case THINKBMS:
        Serial.print("THINKBMS");
        break;
    case SYSTEM:
        Serial.print("SYSTEM");
        break;
    case HEARTBEAT:
        Serial.print("HEARTBEAT");
        break;
    case MEMCACHE:
        Serial.print("MEMCACHE");
        break;
    }
    Serial.print(" - ");
}


