Teensy Threading Library (Enhanced)
===================================================

Teensy Threading Library implements preemptive threads for the Teensy 4
platform from [PJRC](https://www.pjrc.com/teensy/index.html). It supports a
native interface and a `std::thread` interface. Also supports message passing
between threads.

Official repsitory of original library: https://github.com/ftrias/TeensyThreads

Simple example
------------------------------

```C++
#include <TeensyThreads.h>
volatile int count = 0;
void thread_func(int data){
  while(1) count += data;
}
void setup() {
  threads.addThread(thread_func, 1);
}
void loop() {
  Serial.println(count);
}
```

Or using std::thread

```C++
#include <TeensyThreads.h>
volatile int count = 0;
void thread_func(int data){
  while(1) count += data;
}
void setup() {
  std::thread th1(thread_func, 1);
  th1.detach();
}
void loop() {
  Serial.println(count);
}
```

Install
-----------------------------
The easiest way to get started is to download the ZIP file from this repository and install it in Arduino using the menu `Sketch` / `Include Library` / `Add .ZIP Library`.

You can then review the Examples provided `File` / `Examples` / `TeensyThreads`.

Usage
-----------------------------
First, include the library with

```C++
#include <TeensyThreads.h>
```

A global variable `threads` of `class Threads` will be created and used to
control the threading action. Threads are kept in a linked list, so the number of
threads is limited only by available memory.

Threads are created by `threads.addThread()` with parameters:

>`int addThread(func, arg, stack_size, stack)`
>
>- Returns an ID number or -1 for failure
>
>- **func** : function to call to perform thread work. This has the form of `void f(void *arg)` or `void f(int arg)` or `void f()`
>
>- **arg**  : (optional) the `arg` passed to `func` when it starts.
>
>- **stack_size** : (optional) the size of the thread stack. If stack_size is 0 or missing, then 1024 is used.
>
>- **stack** : (optional) pointer to a buffer to use as stack. If stack is 0 or missing, then the buffer is allocated from the heap.

All threads start immediately and run until the function terminates (usually with
a return).

Once a thread ends because the function returns, then the thread will be reused
by a new function.

If a stack has been allocated by the library and not supplied by the caller, it
is allocated together with the thread's internal state, which sits just above
the top of the stack so a stack overflow cannot corrupt it. That memory is kept
when the thread ends and is reused by a later `addThread()` that needs a stack
of the same size or smaller. If the stack was supplied by the caller, the
caller must free it if needed.

A thread that overflows its stack is ended at its next context switch. Overflow
is detected when the stack pointer is at the bottom of the stack, or when the
marker word at the bottom has been overwritten. Memory below the stack may
still have been damaged by then, so size stacks generously: `Serial.printf()`
alone needs well over 1 KB.

The following members of `class Threads` control threads. Items in all caps
are constants in `Threads` and are accessed as in `Threads::EMPTY`.

Threads | Description
--- | ---
int id(); | Get the id of the currently running thread
int getState(int id); | Get the state; see class constants. Can be EMPTY, RUNNING, ENDED, SUSPENDED, SLEEPING, BLOCKED.
int wait(int id, unsigned int timeout_ms = 0) | Wait until thread ends, up to timeout_ms milliseconds. If 0, wait indefinitely.
int kill(int id) | Permanently stop a running thread. Thread will end on the next thread slice tick.
int suspend(int id) |Suspend a thread (on the next slice tick). Can be restarted with restart().
int restart(int id); | Restart a suspended thread.
int setSliceMillis(int milliseconds) | Set each time slice to be 'milliseconds' long
int setSliceMicros(int microseconds) | Set each time slice to be 'microseconds' long
void yield() | Yield current thread's remaining time slice to the next thread, causing immedidate context switch
void delay(int millisecond) | Wait for milliseconds using yield(), giving other slices your wait time
int start(int new_state = -1) | Start/restart threading system; returns previous state. Optionally pass STARTED, STOPPED, FIRST_RUN to restore a different state.
int stop() | Stop threading system; returns previous state: STARTED, STOPPED, FIRST_RUN
**CPU usage** |
float getCPUUsage(int id) | Percentage of CPU time (0-100) used by a thread during the last completed measurement window. Interrupt time is charged to the interrupted thread.
void setCPUUsageWindow(unsigned int ms) | Set the CPU usage measurement window in milliseconds (default 1000, max 4000)
unsigned long getCyclesUsed(int id) | Total CPU cycles used by a thread since it was created (wraps around)
float getSecondsUsed(int id) | Total time in seconds a thread has run since it was created. Does not wrap. Includes time up to the thread's last context switch.
**Advanced functions** |
void setDefaultStackSize(unsigned int bytes_size) | Set the stack size for new threads in bytes
void setTimeSlice(int id, unsigned int ticks) | Set the slice length time in ticks for a thread (1 tick = 1 millisecond, unless using MicroTimer)
void setDefaultTimeSlice(unsigned int ticks) |Set the slice length time in ticks for all new threads (1 tick = 1 millisecond, unless using MicroTimer)
int setMicroTimer(int tick_microseconds = DEFAULT_TICK_MICROSECONDS) | use the microsecond timer provided by IntervalTimer & PIT; instead of 1 tick = 1 millisecond, 1 tick will be the number of microseconds provided (default is 100 microseconds)
**Power saving** |
void idle() | optionally called in main loop to put the CPU to sleep (via the sleep callback) until the next sleeping thread is due
void sleep(int ms) | suspend the current thread for ms milliseconds; it is woken automatically by the scheduler
void setSleepCallback(int (*)(int)) | Set sleep callback function that puts CPU to sleep


In addition, the Threads class has a member class for mutexes (or locks):

Threads::Mutex | Description
--- | ---
int getState() | Get the lock state; 1+=locked; 0=unlocked
int lock(unsigned int timeout_ms = 0) | Lock, optionally waiting up to timeout_ms milliseconds
int try_lock() | If lock available, get it and return 1; otherwise return 0
int unlock() | Unlock if locked

When possible, it's best to use `Threads::Scope` instead of `Threads::Mutex` to ensure orderly locking and unlocking.

Threads::Scope | Description
--- | ---
Scope(Mutex& m) | On creation, mutex is locked
~Scope() | On descruction, it is unlocked

```C++
  // example of manual locking/unlocking
  Threads::Mutex mylock;
  mylock.lock();
  x = 1;
  mylock.unlock();

  // example of automatic locking/unlocking
  if (y) {
    Threads::Scope m(mylock); // lock on creation
    x = 2;
  }                           // unlock at end of scope
```

Message queues
-----------------------------

`Threads::Queue<T, N>` is a thread safe FIFO queue holding up to `N` messages
of type `T`, used to pass data between threads, and from interrupts to threads.
`T` must be trivially copyable (numbers, pointers, plain structs). Storage is
fixed at compile time; nothing is allocated from the heap.

Threads::Queue<T, N> | Description
--- | ---
bool send(const T &item, unsigned int timeout_ms = 0) | Add item to the back, waiting up to timeout_ms milliseconds for space. If 0, wait indefinitely. Returns true if queued.
bool trySend(const T &item) | Add item only if there is space now
bool receive(T &item, unsigned int timeout_ms = 0) | Remove the front item into `item`, waiting up to timeout_ms milliseconds for one. If 0, wait indefinitely. Returns true if received.
bool tryReceive(T &item) | Receive only if an item is available now
bool peek(T &item) | Copy the front item without removing it; false if empty
void clear() | Discard all queued items
unsigned int count() | Number of items queued
unsigned int space() | Number of free slots
unsigned int capacity() | N
bool isEmpty(), isFull() | Queue state

A thread waiting in `send()` or `receive()` is put in the `BLOCKED` state and
uses no CPU time until another thread or interrupt makes room or delivers a
message (or its timeout expires). Any number of threads may send to and
receive from the same queue. Thread 0 (`loop()`) can also wait, but since it
is always scheduled, it polls by yielding rather than truly blocking.

Interrupt handlers should use `trySend()`, `tryReceive()` and `peek()`. A
blocking call made from an interrupt, or with interrupts disabled, never waits
and behaves like its `try` version.

Items are copied with interrupts briefly disabled, so keep messages small;
to pass large buffers, send pointers to them.

```C++
struct Message { int sender; uint32_t value; };
Threads::Queue<Message, 16> inbox;

void worker() {
  Message m;
  while (1) {
    inbox.receive(m);                   // BLOCKED until a message arrives
    Serial.println(m.value);
  }
}

void setup() {
  threads.addThread(worker);
}

void loop() {
  Message m = { 0, millis() };
  if (!inbox.send(m, 100)) Serial.println("inbox full");  // wait up to 100 ms
  threads.delay(1000);
}
```

See the `MessageQueue` example for producers, a consumer and an interrupt
sharing queues.

Usage notes
-----------------------------

The optimizer sometimes has strange side effects because it thinks variables
can't be changed within code. For example, if one thread modifies a variable
and another thread checks for it, the second thread's check may be optimized
away. Here is an example:

```C++
// BUGGY CODE
int state = 0;            // this should be "volatile int state"
void thread1() { state = processData(); }
void run() {
  while (state < 100) {   // this line changed to 'if'
    // do something
  }
}
```

In the code above, seeing that `state` is not changed in the loop, the
optimizer will convert the `while (state<100)` into an `if` statement. Adding
`volatile` to the declaration of `int state` will usually remedy this (as in
`volatile int state`).

```C++
// CORRECT CODE
volatile int state = 0;
void thread1() { state = processData(); }
void run() {
  while (state < 100) {   // this line changed to 'if'
    // do something
  }
}
```

Locking
-----------------------------

Because the Teensy libraries are not usually thread-safe, it's best to  only use
one library in one thread. For example, if using the Wire library, it should
always be used in a single thread. If this cannot be done, then all uses
should be surrounded by locks.

For example:

```C++
  Threads::Mutex wire_lock;

  void init()
  {
    Threads::Scope scope(wire_lock);
    Wire.beginTransmission(17);
    Wire.write(0x1F);
    Wire.write(0x31);
    Wire.endTransmission();
  }
```

Experimental: For widely used libraries like Serial, adding locks may require
vast changes to the code. Because of this, the library provides a helper class
and set of macros to encapsulate all method calls with a lock. An example
illustrates its use:

```C++
// this method is experimental
ThreadWrap(Serial, SerialXtra);
#define Serial ThreadClone(SerialXtra)

int thread_func()
{
    Serial.println("begin");
}
```

In the code above, every time `Serial` is used, it will first lock a mutex,
then call the desired method, then unlock the mutex. This shortcut will only
work on all the code located below the `#define` line. More information
about the mechanics can be found by looking at the source code.


Alternative std::thread interface
-----------------------------

The library also supports the construction of minimal `std::thread` as used
in C++11. `std::thread` always allocates its own stack of the default size. In
addition, a minimal `std::mutex` and `std::lock_guard` are also implemented.
See http://www.cplusplus.com/reference/thread/thread/

Example:

```C++
void run() {
  std::thread first(thread_func);
  first.detach();
}
```

The following members are implemented:

```C++
namespace std {
  class thread {
    bool joinable();
    void detach();
    void join();
    int get_id();
  }
  class mutex {
    void lock();
    bool try_lock();
    void unlock();
  };
  template <class Mutex> class lock_guard {
    lock_guard(Mutex& m);
  }
}
```

Notes on implementation
-----------------------------

Threads take turns on the CPU and are switched by the `context_switch()`
function, written in assembly. This function is called by the SysTick ISR. The
library overrides the default `systick_isr()` to accomplish switching. On the
Teensy by default, each tick is 1 millisecond long. By default, each thread
runs for 100 ticks, or 100 milliseconds, but this can be changed by
`setTimeSlice()`.

Much of the Teensy core software is thread-safe, but not all. When in doubt,
stop and restart threading in critical areas. In general, functions that share
global variables or state should not be called on different threads at the
same time. For example, don't use Serial in two different threads
simultaneously; it's ok to make calls on different threads at different times.

The code comments on the source code give some technical explanation of the
context switch process:

```C
/*
 * context_switch() changes the context to a new thread. It follows this strategy:
 *
 * 1. Abort if called from within an interrupt
 * 2. Save registers r4-r11 to the current thread state (s0-s31 is using FPU)
 * 3. If not running on MSP, save PSP to the current thread state
 * 4. Get the next running thread state
 * 5. Restore r4-r11 from thread state (s0-s31 for FPU)
 * 6. Set MSP or PSP depending on state
 * 7. Switch MSP/PSP on return
 *
 * Notes:
 * - Cortex-M4 has two stack pointers, MSP and PSP, which I alternate. See the
 *   CPU reference manual under the Exception Model section.
 * - I tried coding this in asm embedded in Threads.cpp but the compiler
 *   optimizations kept changing my code and removing lines so I have to use
 *   a separate assembly file. But if you try C++, make sure to declare the
 *   function "naked" so the stack pointer SP is not modified when called.
 *   This means you can't use local variables, which are stored in stack.
 *   Also turn optimizations off using optimize("O0").
 * - Function is called from systick_isr (also naked) via a branch. Again, this is
 *   to preserve the stack and LR. I override the default systick_isr().
 * - Since Systick can be called from within another interrupt, I check
 *   for this and abort.
 * - Teensy uses MSP for it's main thread; I preserve that. Alternatively, I
 *   could have used PSP for all threads, including main, and reserve MSP for
 *   interrupts only. This would simplify the code slightly, but could introduce
 *   incompatabilities.
 * - If this interrupt is nested within another interrupt, all kinds of bad
 *   things can happen. This is especially true if usb_isr() is active. In theory
 *   we should be able to do a switch even within an interrupt, but in my
 *   tests, it would not work reliably.
 */
```

Todo
-----------------------------

- Optimize assembler and other switching code.
- Check for stack overflow during context_change() to aid in debugging;
or have a stack that grows automatically if it gets close filling.
- Fully implement the new C++11 std::thread or POSIX threads.
   See http://www.cplusplus.com/reference/thread/thread/.

Changes
-----------------------------

Revision 0.2: March 2017
1. Change default time slice to 10 milliseconds
2. Add setDefaultTimeSlice(), setDefaultStackSize().
3. Support slices smaller than 1 ms using IntervalTimer; see setMicroTimer().
4. Rename to use TeensyThreads.h to avoid conflicts

Revision 0.3: April 2017
1. Rename Threads::Lock to Threads::Suspend
2. Add setSliceMicros() and setSliceMillis()
3. "lock" will suspend blocking thread until "unlock" and then give thread priority
4. Add ThreadWrap macro and supporting classes
5. Support linking with LTO (link time optimization)

Revision 0.4: July 2017
1. Make ThreadInfo dynamic, saving memory for unused threads

Revision 0.5 Oct 2026
1. Add Message queue system so threads can send messages in a safe way
2. Fixed up many edge cases to work the way things were always supposed to
3. Removed support for Teensy 3 processors (gets rid of some ifdef blocks, tidy up code)
4. Moved CPU usage tracking out of DEBUG and into full time usage. Now it always happens
5. ThreadInfo now stored as a linked list so many threads are possible now (limited by memory)
6. Better tracking of why a thread is not running (sleeping, suspended, blocked on message reading)

Other
-----------------------------

Copyright 2017 by Fernando Trias. See license.txt for details.

Modifications by Claude via Collin Kidder in 2026. So, thank whomever's code Claude
processed/stole to be able to generate the additions. Collin mostly supervised and
tested the code, Claude did most of the work (credit where credit is due!)
