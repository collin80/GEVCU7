/*
 * TickHandler.cpp
 *
 * Class to which TickObserver objects can register to be triggered
 * on a certain interval.
 *
 * NOTE: The initialize() method must be called before a observer is registered !
 *
 Copyright (c) 2013-2026 Collin Kidder, Michael Neuweiler, Charles Galpin

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
#include "devices/Device.h"

extern char _stext[], _etext[];
extern char _ebss[], _heap_end[], *__brkval;

void threadTrampoline(void* param) {
    auto tim = reinterpret_cast<uint32_t>(param);
    while (true) tickHandler.handleInterrupt(static_cast<int>(tim));
}

void memInfo () {
    auto sp = (char*) __builtin_frame_address(0);

    auto stack = sp - _ebss;
    Logger::info("avail STACK %8d b %5d kb", stack, stack >> 10);

    auto heap = _heap_end - __brkval;
    Logger::info("avail HEAP  %8d b %5d kb", heap, heap >> 10);
}



FLASHMEM void   getFreeITCM() { // end of CODE ITCM, skip full 32 bits
    uint32_t *ptrFreeITCM;  // Set to Usable ITCM free RAM
    uint32_t  sizeofFreeITCM; // sizeof free RAM in uint32_t units.
    uint32_t  SizeLeft_etext;

    SizeLeft_etext = (32 * 1024) - (((uint32_t)&_etext - (uint32_t)&_stext) % (32 * 1024));
    sizeofFreeITCM = SizeLeft_etext - 4;
    sizeofFreeITCM /= sizeof(ptrFreeITCM[0]);
    Logger::info("Size of Free ITCM in Bytes = %u", sizeofFreeITCM * sizeof(ptrFreeITCM[0]));
}

TickHandler::TickHandler()
{
    for (int i = 0; i < NUM_THREADS; i++) {
        threadEntry[i].interval = 0;
        threadEntry[i].isRunning = false;
        threadEntry[i].detached = false;
        threadEntry[i].observer = NULL;
    }
}

FLASHMEM void TickHandler::setup()
{
}


/// @brief
/// @param p 
/// @param arg 
/// @param stack_size 
/// @param stack 
/// @param name 
/// @param interval
/// @details Directly adds a thread to the system with the specified parameters. Fills in both types of thread bookkeeping.
int TickHandler::directAddThread(ThreadFunction p, void * arg, int stack_size, void *stack, const char *name, uint32_t interval)
{
    RecursiveScope m(tickMutex);
    int thread;
    thread = findFreeThread(false); //can't reuse an idle tick thread here as it runs threadTrampoline, not p
    if (thread == -1) {
        Logger::error("No free thread available for main thread with interval=%d", interval);
        return -1;
    }
    threadEntry[thread].interval = interval;
    threadEntry[thread].isRunning = false;
    threadEntry[thread].name = name ? String(name) : String("");
    threadEntry[thread].observer = nullptr; //not used in this case

    //HERE NEED TO START THE THREAD IF IT IS NOT ALREADY RUNNING.
    if (!threadEntry[thread].isRunning) {
        Logger::debug("Starting main thread %d with interval %dus", thread + 1, interval);
        threads.addThread(p, arg, stack_size, nullptr);
        threadEntry[thread].isRunning = true;
    }
    return thread;
}

/**
 * Register an observer to be triggered in a certain interval.
 * TickObservers with the same interval are grouped to one thread to save resources.
 * A TickObserver may be registered multiple times with different intervals.
 *
 * First a thread with the same interval is looked up. If none found, a free one is
 * used. Then a free TickObserver slot (of max CFG_MAX_TICK_OBSERVERS) is looked up. If all went
 * well, the thread is configured and (re)started.
 */
FLASHMEM void TickHandler::attach(TickObserver* observer, uint32_t interval, const char *name) {
    RecursiveScope m(tickMutex);
    int thread;
    thread = findFreeThread(true); //prefers an idle thread left behind by detach() before starting a new one
    if (thread == -1) {
        Logger::error("No free thread available for interval=%d", interval);
        return;
    }
    threadEntry[thread].interval = interval;
    threadEntry[thread].name = name ? String(name) : String("");
    threadEntry[thread].detached = false;
    //set last. If this slot is being reused its thread is already running and will pick this up on its next loop
    threadEntry[thread].observer = observer;
    Logger::debug("attached TickObserver (%X) to thread %d, %dus interval", observer, thread + 1, interval);

    //HERE NEED TO START THE THREAD IF IT IS NOT ALREADY RUNNING.
    if (!threadEntry[thread].isRunning) {
        Logger::debug("Starting thread %d with interval %dus", thread + 1, interval);
        threads.addThread(threadTrampoline, reinterpret_cast<void*>(static_cast<uintptr_t>(thread)), 2048, nullptr);
        threadEntry[thread].isRunning = true;
    }
}

/**
 * Remove an observer from all timers where it was registered.
 * The thread itself keeps running but idles (it no longer calls anything) and attach() will reuse it.
 * Note that if the observer's handleTick() was already running when this is called it will still finish.
 */
FLASHMEM void TickHandler::detach(TickObserver* observer) {
    if (!observer) 
    {
        Logger::debug("Attempt to call TickHandler::detach with a null ptr!");
        return;
    }
    RecursiveScope m(tickMutex);
    for (int thread = 0; thread < NUM_THREADS; thread++) {
        if (threadEntry[thread].observer == observer) {
            Logger::debug("removing TickObserver (%X) from thread %d", observer, thread + 1);
            threadEntry[thread].observer = nullptr;
            threadEntry[thread].detached = true;
        }
    }
}

/* Find a free slot. If reuseIdle is set then a running tick thread whose observer was detached
   is used first. Otherwise (or if there are none) an entry that has never been used is returned.
   Must be called with tickMutex held.
*/
int TickHandler::findFreeThread(bool reuseIdle)
{
    if (reuseIdle)
    {
        for (int i = 0; i < NUM_THREADS; i++)
        {
            if (threadEntry[i].detached && threadEntry[i].isRunning) return i;
        }
    }
    for (int i = 0; i < NUM_THREADS; i++)
    {
        //find empty thread entry
        if (threadEntry[i].interval == 0)
        {
            return i;
        }
    }
    return -1;
}

/*
 * Handle the time elapsed for each thread
 * All the registered TickObservers of the thread are called.
 */
void TickHandler::handleInterrupt(int threadNumber) {
    uint32_t now = micros();
    //read it only once. detach() can clear it from another thread at any moment
    TickObserver *observer = threadEntry[threadNumber].observer;
    if (observer != NULL) {
        observer->handleTick();
    }
    uint32_t elapsed = micros() - now;
    //each tread must delay for the appropriate amount of time before the next tick.
    //This is done here so that the thread can be used for multiple observers with the same interval.
    //we track the total time spent doing the work and then delay for the remainder of the interval. 
    //If the work took longer than the interval, we log a warning.
    if (elapsed > threadEntry[threadNumber].interval) {
        Logger::warn("TickHandler thread %d took %u us to process, longer than interval of %u us", threadNumber + 1, elapsed, threadEntry[threadNumber].interval);
    }
    else threads.delay(threadEntry[threadNumber].interval / 1000 - elapsed / 1000);
}

void TickHandler::showThreadInfo()
{
    float threadTime;
    Logger::info("Threading system runs at 100us tick interval. This is every 60,000 instructions.");
    Logger::info("Thread  0: IdleLoop             State: RUNNING    interval: continuous CPU: %4.1f%% Time: %4.2fs", threads.getCPUUsage(0), threads.getSecondsUsed(0));
    for (int i = 0; i < NUM_THREADS; i++)
    {
        if (threadEntry[i].interval > 0)
        {
            int state = threads.getState(i + 1);
            const char *stateStr;
            switch (state)
            {
            case Threads::RUNNING:
                stateStr = "RUNNING";
                break;
            case Threads::ENDED:
                stateStr = "ENDED";
                break;
            case Threads::ENDING:
                stateStr = "ENDING";
                break;
            case Threads::SUSPENDED:
                stateStr = "SUSPENDED";
                break;
            case Threads::SLEEPING:
                stateStr = "SLEEPING";
                break;
            case Threads::BLOCKED:
                stateStr = "BLOCKED";
                break;
            default:
                stateStr = "UNKNOWN";
                break;
            }
            threadTime = threads.getSecondsUsed(i + 1);
            Logger::info("Thread %2d: %-20s State: %-10s interval: %8dus CPU: %4.1f%% Time: %4.2fs", i + 1, threadEntry[i].name.c_str(), stateStr, threadEntry[i].interval, threads.getCPUUsage(i + 1), threadTime);
        }
    }
    //now display the memory usage info for the system. This is useful to see if we are running out of memory or stack space.
    //stack is ITCM and heap is DTCM. The stack is used for function calls and local variables. 
    //The heap is used for dynamic memory allocation. 
    //If the stack or heap is too small, the system will crash or behave unpredictably.
    memInfo();
    //free ITCM is of nearly no practical use to anyone. Could show it but it's not important.
    //getFreeITCM();
}

/*
 * Default implementation of the TickObserver method. Must be overwritten
 * by every sub-class.
 */
void TickObserver::handleTick() {
    Logger::error("TickObserver does not implement handleTick()");
}

TickHandler tickHandler;
