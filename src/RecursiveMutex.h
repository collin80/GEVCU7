/*
 * RecursiveMutex.h
 *
 * A re-entrant wrapper around Threads::Mutex. The TeensyThreads mutex does not track
 * which thread owns it so if a thread tries to lock a mutex it already holds it will
 * wait forever on itself. This wrapper remembers the owning thread and a lock count
 * so the owner can lock again freely (for instance, a CAN receive callback that sends
 * a frame while canEvents() already holds the bus mutex). The underlying mutex is only
 * released when the owner has unlocked as many times as it locked.

Copyright (c) 2026 Collin Kidder

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

#ifndef RECURSIVEMUTEX_H_
#define RECURSIVEMUTEX_H_

#include <TeensyThreads.h>

class RecursiveMutex {
public:
    int lock()
    {
        int me = threads.id();
        //owner can only ever equal our own id if we set it ourselves and haven't released yet
        //so it's safe to check this without holding anything.
        if (owner == me)
        {
            count++;
            return 1;
        }
        mutex.lock();
        owner = me;
        count = 1;
        return 1;
    }

    int unlock()
    {
        if (owner != threads.id()) return 0; //not ours to unlock
        if (--count == 0)
        {
            owner = -1; //must be cleared before the real unlock so the next owner doesn't see us
            mutex.unlock();
        }
        return 1;
    }

private:
    Threads::Mutex mutex;
    volatile int owner = -1;
    volatile int count = 0;
};

//same idea as Threads::Scope - lock on construction, unlock when it goes out of scope
class RecursiveScope {
public:
    explicit RecursiveScope(RecursiveMutex &m) : r(m) { r.lock(); }
    ~RecursiveScope() { r.unlock(); }
private:
    RecursiveMutex &r;
};

#endif /* RECURSIVEMUTEX_H_ */
