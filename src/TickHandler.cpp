/*
 * TickHandler.cpp
 *
 * Class to which TickObserver objects can register to be triggered
 * on a certain interval.
 * TickObserver with the same interval are grouped to the same timer
 * and triggered in sequence per timer interrupt.
 *
 * NOTE: The initialize() method must be called before a observer is registered !
 *
 Copyright (c) 2021 Collin Kidder, Michael Neuweiler, Charles Galpin

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

#include "TickHandler.h"

int timer;
//we're using up to 9 timers and they are defined here so that they can
//explicitly be set to the various hardware timers. This is used to spread out
//which timers are used and to allow a wide range of max durations
Timer t1(GPT1);
Timer t2(GPT2);
Timer t3(TMR4);
Timer t4(TMR4);
Timer t5(PIT);
Timer t6(TCK); //these TCK timers can be efficient cycle-wise but
Timer t7(TCK); //they require that the yield() function is called
Timer t8(TCK); //which makes them sensitive to long running tasks
Timer t9(TCK); //on the main execution thread. So, they're used last


void timerTrampoline(int tim) {
    tickHandler.handleInterrupt(tim);
}

void emptyTimerInt()
{

}

TickHandler::TickHandler() {
    timers[0] = &t1;
    timers[1] = &t2;
    timers[2] = &t3;
    timers[3] = &t4;
    timers[4] = &t5;
    timers[5] = &t6;
    timers[6] = &t7;
    timers[7] = &t8;
    timers[8] = &t9;
    for (int i = 0; i < NUM_TIMERS; i++) {
        timerEntry[i].interval = 0;
        for (int j = 0; j < CFG_TIMER_NUM_OBSERVERS; j++) {
            timerEntry[i].observer[j] = NULL;
        }
    }
#ifdef CFG_TIMER_USE_QUEUING
    bufferHead = bufferTail = 0;
#endif
}

void TickHandler::setup()
{
    for (int i = 0; i < NUM_TIMERS; i++)
    {
        timers[i]->begin(emptyTimerInt, 100000);
        timerEntry[i].maxInterval = (long)(timers[i]->getMaxPeriod() * 1000000.0);
        Logger::console("Timer %i max interval is: %i", i, timerEntry[i].maxInterval);
        timers[i]->stop(); //don't keep idle timers running
    }
}

/**
 * Register an observer to be triggered in a certain interval.
 * TickObservers with the same interval are grouped to one timer to save timers.
 * A TickObserver may be registered multiple times with different intervals.
 *
 * First a timer with the same interval is looked up. If none found, a free one is
 * used. Then a free TickObserver slot (of max CFG_MAX_TICK_OBSERVERS) is looked up. If all went
 * well, the timer is configured and (re)started.
 */
void TickHandler::attach(TickObserver* observer, uint32_t interval) {
    timer = findTimer(interval);
    if (timer == -1) {
        timer = findTimer(0);	// no timer with given tick interval exist -> look for unused (interval == 0)
        if (timer == -1) {
            Logger::error("No free timer available for interval=%d", interval);
            return;
        }
        timerEntry[timer].interval = interval;
    }

    int observerIndex = findObserver(timer, 0);
    if (observerIndex == -1) {
        Logger::error("No free observer slot for timer %d with interval %d", timer, timerEntry[timer].interval);
        return;
    }
    timerEntry[timer].observer[observerIndex] = observer;
    Logger::debug("attached TickObserver (%X) as number %d to timer %d, %dus interval", observer, observerIndex, timer, interval);
    //I might be dumb but using a line like:
    // timers[timer]->beginPeriodic([timer]() { timerTrampoline(timer); }, interval);
    // doesn't work. Instead the value passed by the lambda function ends up always being the last timer you made
    // instead of each timer passing its own number. This is likely a failure of syntax on my part.
    // This switch construct works around my ignorance. Anyone seeing this who knows C++ lambda functions
    // better than I apparently do is free to correct the syntax and shrink this down to one line again.
    switch (timer)
    {
    case 0:
        timers[0]->beginPeriodic([]() { timerTrampoline(0); }, interval);
        break;
    case 1:
        timers[1]->beginPeriodic([]() { timerTrampoline(1); }, interval);
        break;
    case 2:
        timers[2]->beginPeriodic([]() { timerTrampoline(2); }, interval);
        break;
    case 3:
        timers[3]->beginPeriodic([]() { timerTrampoline(3); }, interval);
        break;
    case 4:
        timers[4]->beginPeriodic([]() { timerTrampoline(4); }, interval);
        break;
    case 5:
        timers[5]->beginPeriodic([]() { timerTrampoline(5); }, interval);
        break;
    case 6:
        timers[6]->beginPeriodic([]() { timerTrampoline(6); }, interval);
        break;
    case 7:
        timers[7]->beginPeriodic([]() { timerTrampoline(7); }, interval);
        break;
    case 8:
        timers[8]->beginPeriodic([]() { timerTrampoline(8); }, interval);
        break;
    }
    
}

/**
 * Remove an observer from all timers where it was registered.
 */
void TickHandler::detach(TickObserver* observer) {
    for (int timer = 0; timer < NUM_TIMERS; timer++) {
        for (int observerIndex = 0; observerIndex < CFG_TIMER_NUM_OBSERVERS; observerIndex++) {
            if (timerEntry[timer].observer[observerIndex] == observer) {
                Logger::debug("removing TickObserver (%X) as number %d from timer %d", observer, observerIndex, timer);
                timerEntry[timer].observer[observerIndex] = NULL;
                timers[timer]->stop();
            }
        }
    }
}

/**
 * Find a timer with a specified interval.
 */
int TickHandler::findTimer(long interval) {
    for (int i = 0; i < NUM_TIMERS; i++) {
        if (timerEntry[i].interval == interval)
            return i;
    }
    return -1;
}

/*
 * Find a TickObserver in the list of a specific timer.
 */
int TickHandler::findObserver(int timer, TickObserver *observer) {
    for (int i = 0; i < CFG_TIMER_NUM_OBSERVERS; i++) {
        if (timerEntry[timer].observer[i] == observer)
            return i;
    }
    return -1;
}

#ifdef CFG_TIMER_USE_QUEUING
/*
 * Check if a tick is available, forward it to registered observers.
 */
void TickHandler::process() {
    while (bufferHead != bufferTail) {
        tickBuffer[bufferTail]->handleTick();
        bufferTail = (bufferTail + 1) % CFG_TIMER_BUFFER_SIZE;
        //Logger::debug("process, bufferHead=%d bufferTail=%d", bufferHead, bufferTail);
    }
}

void TickHandler::cleanBuffer() {
    bufferHead = bufferTail = 0;
}

#endif //CFG_TIMER_USE_QUEUING

/*
 * Handle the interrupt of any timer.
 * All the registered TickObservers of the timer are called.
 */
void TickHandler::handleInterrupt(int timerNumber) {
    for (int i = 0; i < CFG_TIMER_NUM_OBSERVERS; i++) {
        if (timerEntry[timerNumber].observer[i] != NULL) {
#ifdef CFG_TIMER_USE_QUEUING
            tickBuffer[bufferHead] = timerEntry[timerNumber].observer[i];
            bufferHead = (bufferHead + 1) % CFG_TIMER_BUFFER_SIZE;
            //Logger::debug("TN: %i bufferHead=%d, bufferTail=%d, observer=%x", timerNumber, bufferHead, bufferTail, timerEntry[timerNumber].observer[i]);
#else
            timerEntry[timerNumber].observer[i]->handleTick();
#endif //CFG_TIMER_USE_QUEUING
        }
    }
}

/*
 * Default implementation of the TickObserver method. Must be overwritten
 * by every sub-class.
 */
void TickObserver::handleTick() {
    Logger::error("TickObserver does not implement handleTick()");
}

TickHandler tickHandler;
