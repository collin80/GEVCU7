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

void timerTrampoline(int tim) {
    tickHandler.handleInterrupt(tim);
}

TickHandler::TickHandler() {
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

    timers[timer].beginPeriodic([] { timerTrampoline(timer); }, interval);
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
//Logger::debug("bufferHead=%d, bufferTail=%d, observer=%d", bufferHead, bufferTail, timerEntry[timerNumber].observer[i]);
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
