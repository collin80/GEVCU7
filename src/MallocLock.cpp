/*
 * MallocLock.cpp
 *
 * Makes the newlib heap safe to use from multiple TeensyThreads threads.
 *
 * newlib calls __malloc_lock / __malloc_unlock around every malloc, free, realloc, etc.
 * The default versions in the toolchain do nothing at all so a context switch partway
 * through an allocation can corrupt the heap. Every new, String, and ArduinoJson document
 * goes through here so this matters a lot. Instead, pause thread switching for the (very short)
 * duration of the heap operation. Interrupts stay enabled the whole time.
 *
 * A mutex is not used on purpose. Global constructors allocate memory before the threads object
 * has been constructed and threads.stop() / threads.start() only touch a plain global so they are
 * safe that early. Also, nothing inside malloc ever waits on another thread so there's no risk here.
 *
 * NOTE: This does not protect against malloc being called from within an interrupt handler.
 * Never allocate memory from an ISR.

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

#include <TeensyThreads.h>
#include <reent.h>

//Only ever touched while thread switching is stopped so no other thread can see these change.
//Has to be recursive as newlib can lock again while already holding the lock (realloc for instance)
static int mallocLockDepth = 0;
static int mallocPrevState;

//used attribute keeps LTO from deciding these aren't needed and falling back to the do-nothing newlib versions
extern "C" __attribute__((used)) void __malloc_lock(struct _reent *)
{
    int prev = threads.stop();
    if (mallocLockDepth++ == 0) mallocPrevState = prev;
}

extern "C" __attribute__((used)) void __malloc_unlock(struct _reent *)
{
    if (--mallocLockDepth == 0) threads.start(mallocPrevState);
}
