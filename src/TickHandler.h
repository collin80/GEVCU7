/*
 * TickHandler.h
 *
 * Class where TickObservers can register to be triggered
 * on a certain interval.
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

#ifndef TICKHANDLER_H_
#define TICKHANDLER_H_

#include "config.h"
#include <TeensyTimerTool.h>
#include "Logger.h"

using namespace TeensyTimerTool;

//its technically possible to register even more timers but the higher you go the more likely
//you are to clobber timers being used by other things.
#define NUM_TIMERS 12

class TickObserver {
public:
    virtual void handleTick();
};


class TickHandler {
public:
    TickHandler();
    void setup();
    void attach(TickObserver *observer, uint32_t interval);
    void detach(TickObserver *observer);
    void handleInterrupt(int timerNumber); // must be public when from the non-class functions
#ifdef CFG_TIMER_USE_QUEUING
    void cleanBuffer();
    void process();
#endif

protected:

private:
    struct TimerEntry {
        long interval; // interval of timer in microseconds
        uint64_t maxInterval; //maximum achieveable interval for this timer (in microseconds)
        TickObserver *observer[CFG_TIMER_NUM_OBSERVERS]; // array of pointers to observers with this interval
    };
    TimerEntry timerEntry[NUM_TIMERS]; // array of timer entries
#ifdef CFG_TIMER_USE_QUEUING
    TickObserver *tickBuffer[CFG_TIMER_BUFFER_SIZE];
    volatile uint16_t bufferHead, bufferTail;
#endif
    
    int findTimer(long interval);
    int findFreeTimer(uint64_t interval);
    int findObserver(int timerNumber, TickObserver *observer);

    PeriodicTimer* timers[NUM_TIMERS];
};

extern TickHandler tickHandler;

#endif /* TICKHANDLER_H_ */


