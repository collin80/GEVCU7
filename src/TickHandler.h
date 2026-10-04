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
#include <TeensyThreads.h>
#include "Logger.h"

/*No longer using timers. Using threads instead. Also not grouping devices
with same delay together. Each device can have its own thread and delay.
This allows each thread to run independently and if it crashes it won't take down other threads.
This will use up more CPU time and memory but hopefully there's enough of each
*/
#define NUM_THREADS 32

class TickObserver {
public:
    virtual void handleTick();
};

class TickHandler {
public:
    TickHandler();
    void setup();
    void attach(TickObserver *observer, uint32_t interval, const char *name = nullptr); // interval in microseconds
    void detach(TickObserver *observer);
    void handleInterrupt(int timerNumber); // must be public when from the non-class functions
    void showThreadInfo();
    int directAddThread(ThreadFunction p, void * arg, int stack_size, void *stack, const char *name, uint32_t interval = 100000);

protected:

private:
    struct ThreadEntry {
        long interval; // interval of timer in microseconds
        uint64_t maxInterval; //maximum achieveable interval for this timer (in microseconds)
        TickObserver *observer; // pointer to observer that should be called
        String name; // name of the thread for debugging purposes
        bool isRunning; // true if the timer is running, false if not
    };
    ThreadEntry threadEntry[NUM_THREADS]; // array of timer entries
    
    int findFreeThread(uint64_t interval);
};

extern TickHandler tickHandler;

#endif /* TICKHANDLER_H_ */


