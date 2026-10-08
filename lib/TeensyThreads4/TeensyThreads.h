/*
 * Threads.h - Library for threading on the Teensy.
 *
 *******************
 * 
 * Copyright 2017 by Fernando Trias.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
 * and associated documentation files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, 
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or 
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *******************
 *
 * Multithreading library for Teensy board.
 * See Threads.cpp for explanation of internal functions.
 *
 * A global variable "threads" of type Threads will be created
 * to provide all threading functions. See example below:
 *
 *   #include <Threads.h>
 *
 *   volatile int count = 0;
 *
 *   void thread_func(int data){
 *     while(1) count++;
 *   }
 *
 *   void setup() {
 *     threads.addThread(thread_func, 0);
 *   }
 *
 *   void loop() {
 *     Serial.print(count);
 *   }
 *
 * Alternatively, you can use the std::threads class defined
 * by C++11
 *
 *   #include <Threads.h>
 *
 *   volatile int count = 0;
 *
 *   void thread_func(){
 *     while(1) count++;
 *   }
 *
 *   void setup() {
 *     std::thead th1(thread_func);
 *     th1.detach();
 *   }
 *
 *   void loop() {
 *     Serial.print(count);
 *   }
 *
 */

#ifndef _THREADS_H
#define _THREADS_H

#include <stdint.h>
#include <stddef.h>
#include <type_traits>

extern "C" {
  void context_switch(void);
  void context_switch_direct(void);
  void context_switch_direct_active(void);
  void loadNextThread();
  void stack_overflow_isr(void);
  void threads_svcall_isr(void);
  void threads_systick_isr(void);
}

// The stack frame saved by the interrupt
typedef struct {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t xpsr;
} interrupt_stack_t;

// The stack frame saved by the context switch
typedef struct {
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t lr;
#ifdef __ARM_PCS_VFP
  uint32_t s0;
  uint32_t s1;
  uint32_t s2;
  uint32_t s3;
  uint32_t s4;
  uint32_t s5;
  uint32_t s6;
  uint32_t s7;
  uint32_t s8;
  uint32_t s9;
  uint32_t s10;
  uint32_t s11;
  uint32_t s12;
  uint32_t s13;
  uint32_t s14;
  uint32_t s15;
  uint32_t s16;
  uint32_t s17;
  uint32_t s18;
  uint32_t s19;
  uint32_t s20;
  uint32_t s21;
  uint32_t s22;
  uint32_t s23;
  uint32_t s24;
  uint32_t s25;
  uint32_t s26;
  uint32_t s27;
  uint32_t s28;
  uint32_t s29;
  uint32_t s30;
  uint32_t s31;
  uint32_t fpscr;
#endif
} software_stack_t;

// The state of each thread (including thread 0)
//
// When the library allocates a thread's stack, the ThreadInfo is placed in
// the same block, directly above the top of the stack:
//
//   low address  [ marker | stack (grows down) <- ][ ThreadInfo ]  high address
//
// so a thread that overflows its stack cannot overwrite its own ThreadInfo.
class ThreadInfo {
  public:
    int stack_size;
    uint8_t *stack=0;
    int my_stack = 0;                    // 1 if stack is this node's own block_stack
    uint8_t *block_stack = 0;            // stack area allocated with this node, if any
    int block_stack_size = 0;            // size of block_stack in bytes
    software_stack_t save;
    volatile int flags = 0;
    void *sp;
    int ticks;
    volatile uint32_t wake_time = 0;     // millis() value at which a SLEEPING (or timed BLOCKED) thread wakes
    const void * volatile wait_obj = NULL; // object a BLOCKED thread is waiting on
    volatile bool wait_timed = false;    // true if a BLOCKED thread should wake at wake_time
    unsigned long cyclesAccum = 0;       // total CPU cycles used (wraps around)
    uint32_t cyclesWindow = 0;           // cycles used in the current CPU usage window
    uint32_t cyclesLastWindow = 0;       // cycles used in the last completed CPU usage window
    float secondsAccum = 0;              // total run time in seconds, excluding the current cyclesWindow
    int id = 0;                          // thread id returned by addThread()
    volatile uint32_t generation = 0;    // incremented each time addThread() (re)uses this node
    ThreadInfo *next = NULL;             // next thread in the list (NULL at end)
};

extern "C" void unused_isr(void);

typedef void (*ThreadFunction)(void*);
typedef void (*ThreadFunctionInt)(int);
typedef void (*ThreadFunctionNone)();
typedef int (*ThreadFunctionSleep)(int);

typedef void (*IsrFunction)();

/*
 * Threads handles all the threading interaction with users. It gets
 * instantiated in a global variable "threads".
 */
class Threads {
public:
  int DEFAULT_TICKS = 10;
  int DEFAULT_STACK_SIZE = 1024;
  static const int DEFAULT_STACK0_SIZE = 10240; // estimate for thread 0?
  static const int DEFAULT_TICK_MICROSECONDS = 100;
  static const int UTIL_STATE_NAME_DESCRIPTION_LENGTH = 24;
  static const int UTIL_TRHEADS_BUFFER_LENGTH = 1024;
  static const int DEFAULT_CPU_WINDOW_MS = 1000;
  static const int MAX_CPU_WINDOW_MS = 4000; // window in cycles must fit in 32 bits


  // State of threading system
  static const int STARTED = 1;
  static const int STOPPED = 2;
  static const int FIRST_RUN = 3;

  // State of individual threads
  static const int EMPTY = 0;
  static const int RUNNING = 1;
  static const int ENDED = 2;
  static const int ENDING = 3;
  static const int SUSPENDED = 4;
  static const int SLEEPING = 5;
  static const int BLOCKED = 6;   // waiting on a Queue (or other wait object)

  static const int SVC_NUMBER = 0x21;
  static const int SVC_NUMBER_ACTIVE = 0x22;

protected:
  int current_thread;
  int thread_count;
  int thread_error;

  /*
   * Singly linked list of all threads, ordered by id. The head is always
   * thread 0 (the main MSP thread). Nodes are never freed; a thread that has
   * ENDED keeps its node (and id) so it can be reused by a later addThread().
   * New nodes are only appended to the tail, so the context switcher can
   * safely walk the list at any time.
   */
  ThreadInfo *threadp;

  ThreadFunctionSleep enter_sleep_callback = NULL;

  // CPU usage accounting, updated on every context switch
  uint32_t cpu_switch_start = 0;        // cycle count when current thread was switched in
  uint32_t cpu_window_start = 0;        // cycle count when the current window began
  uint32_t cpu_last_window_cycles = 0;  // length of the last completed window in cycles
  uint32_t cpu_window_ms = DEFAULT_CPU_WINDOW_MS;

public: // public for debugging
  static IsrFunction save_systick_isr;
  static IsrFunction save_svcall_isr;

public:
  Threads();

  // Create a new thread for function "p", passing argument "arg". If stack is 0,
  // stack allocated on heap. Function "p" has form "void p(void *)".
  int addThread(ThreadFunction p, void * arg=0, int stack_size=-1, void *stack=0);
  // For: void f(int)
  int addThread(ThreadFunctionInt p, int arg=0, int stack_size=-1, void *stack=0) {
    return addThread((ThreadFunction)p, (void*)arg, stack_size, stack);
  }
  // For: void f()
  int addThread(ThreadFunctionNone p, int arg=0, int stack_size=-1, void *stack=0) {
    return addThread((ThreadFunction)p, (void*)arg, stack_size, stack);
  }

  // Get the state; see class constants. Can be EMPTY, RUNNING, etc.
  int getState(int id);
  // Explicityly set a state. See getState(). Call with care.
  int setState(int id, int state);
  // Wait until thread ends (returns or is killed), up to timeout_ms milliseconds.
  // If ms is 0, wait indefinitely. Returns id, or -1 on timeout or if called
  // with the current thread's own id.
  int wait(int id, unsigned int timeout_ms = 0);
  // Put the CPU to sleep (via the sleep callback) until the next sleeping
  // thread is due to wake. Optional; call repeatedly from the main loop.
  void idle();
  // Suspend execution of current thread for ms milliseconds. The thread
  // is woken automatically by the scheduler; no sleep callback is needed.
  void sleep(int ms);
  // Permanently stop a running thread. Thread will end on the next thread slice tick.
  int kill(int id);
  // Suspend a thread (on the next slice tick). Can be restarted with restart().
  int suspend(int id);
  // Restart a suspended thread.
  int restart(int id);
  // Set the slice length time in ticks for a thread (1 tick = 1 millisecond, unless using MicroTimer)
  void setTimeSlice(int id, unsigned int ticks);
  // Set the slice length time in ticks for all new threads (1 tick = 1 millisecond, unless using MicroTimer)
  void setDefaultTimeSlice(unsigned int ticks);
  // Set the stack size for new threads in bytes
  void setDefaultStackSize(unsigned int bytes_size);
  // Use the microsecond timer provided by IntervalTimer & PIT; instead of 1 tick = 1 millisecond,
  // 1 tick will be the number of microseconds provided (default is 100 microseconds)
  int setMicroTimer(int tick_microseconds = DEFAULT_TICK_MICROSECONDS);
  // Simple function to set each time slice to be 'milliseconds' long
  int setSliceMillis(int milliseconds);
  // Set each time slice to be 'microseconds' long
  int setSliceMicros(int microseconds);
  // Set sleep callback function
  void setSleepCallback(ThreadFunctionSleep callback);

  // Get the id of the currently running thread
  int id();
  int getStackUsed(int id);
  int getStackRemaining(int id);
  char* threadsInfo(void);
  // Total CPU cycles used by a thread since it was created (wraps around)
  unsigned long getCyclesUsed(int id);
  // Total time in seconds a thread has run since it was created (does not wrap)
  float getSecondsUsed(int id);
  // Percentage of CPU time (0-100) used by a thread during the last completed
  // measurement window. Time spent in interrupts is charged to the interrupted thread.
  float getCPUUsage(int id);
  // Set the CPU usage measurement window in milliseconds (default 1000, max 4000)
  void setCPUUsageWindow(unsigned int ms);

  // Yield current thread's remaining time slice to the next thread, causing immediate
  // context switch
  static void yield();
  // Wait for milliseconds using yield(), giving other slices your wait time
  void delay(int millisecond);
  // Wait for microseconds using yield(), giving other slices your wait time
  void delay_us(int microsecond);
  
  // Start/restart threading system; returns previous state: STARTED, STOPPED, FIRST_RUN
  // can pass the previous state to restore
  int start(int old_state = -1);
  // Stop threading system; returns previous state: STARTED, STOPPED, FIRST_RUN
  int stop();

  // Test all stack markers; if ok return 0; problems, return -1 and set *threadid to id
  int testStackMarkers(int *threadid = NULL);

  // Allow these static functions and classes to access our members
  friend void context_switch(void);
  friend void context_switch_direct(void);
  friend void threads_svcall_isr(void);
  friend void loadNextThread();
  friend class ThreadLock;

protected:
  void getNextThread();
  ThreadInfo *getThreadInfo(int id);
  void *loadstack(ThreadFunction p, void * arg, void *stackaddr, int stack_size);
  static void force_switch_isr();
  void setStackMarker(void *stack);
  ThreadInfo *newThreadInfo(int stack_size);
  uint8_t *stackPointer(ThreadInfo *tp);

private:
  static void del_process(void);
  void yield_and_start();

  // Blocking primitives used by Queue. block() and wake() must be called with
  // interrupts disabled; waitWhileBlocked() must be called with them enabled.
  void block(const void *obj, unsigned int timeout_ms);
  void waitWhileBlocked();
  void wake(const void *obj);

public:
  class Mutex {
  private:
    volatile int state = 0;
    volatile int waitthread = -1;
    volatile int waitcount = 0;
  public:
    int getState(); // get the lock state; 1=locked; 0=unlocked
    int lock(unsigned int timeout_ms = 0); // lock, optionally waiting up to timeout_ms milliseconds
    int try_lock(); // if lock available, get it and return 1; otherwise return 0
    int unlock();   // unlock if locked
  };

  /*
   * QueueBase is the untyped core of Queue: a fixed capacity ring buffer
   * of item_size byte items. Use Queue<T, N> instead of this directly.
   *
   * All operations are safe to call from any thread. trySend(), tryReceive()
   * and peek() are also safe from interrupts. A blocking call made from an
   * interrupt (or with interrupts disabled) behaves like its try version.
   *
   * Items are copied with interrupts disabled, so keep them small; to pass
   * large messages, send pointers to them instead.
   */
  class QueueBase {
  public:
    // Copy item onto the back of the queue, waiting up to timeout_ms
    // milliseconds for space. If timeout_ms is 0, wait indefinitely.
    // Returns true if the item was queued.
    bool send(const void *item, unsigned int timeout_ms = 0);
    // Queue item only if there is space right now
    bool trySend(const void *item);
    // Remove the front item into *item, waiting up to timeout_ms
    // milliseconds for one to arrive. If timeout_ms is 0, wait indefinitely.
    // Returns true if an item was received.
    bool receive(void *item, unsigned int timeout_ms = 0);
    // Receive only if an item is available right now
    bool tryReceive(void *item);
    // Copy the front item into *item without removing it; false if empty
    bool peek(void *item);
    // Discard all queued items
    void clear();

    unsigned int count();     // number of items queued
    unsigned int space();     // number of free slots
    unsigned int capacity() { return cap; }
    bool isEmpty() { return count() == 0; }
    bool isFull() { return space() == 0; }

  protected:
    QueueBase(uint8_t *buffer, size_t item_size, unsigned int capacity);

  private:
    QueueBase(const QueueBase &) = delete;
    QueueBase &operator=(const QueueBase &) = delete;
    bool put(const void *item, unsigned int timeout_ms, bool can_wait);
    bool get(void *item, unsigned int timeout_ms, bool can_wait);

    uint8_t *buf;
    size_t item_size;
    unsigned int cap;
    volatile unsigned int head = 0;   // index of the front item
    volatile unsigned int used = 0;   // number of items queued
    // Only the addresses of these matter: they identify what a BLOCKED thread waits for
    uint8_t readers = 0;              // threads waiting for an item
    uint8_t writers = 0;              // threads waiting for space
  };

  /*
   * Thread safe FIFO message queue holding up to N items of type T.
   * T must be trivially copyable (plain structs, numbers, pointers).
   *
   *   Threads::Queue<Message, 16> q;
   *   q.send(msg);              // waits while full
   *   if (q.receive(msg, 100))  // waits up to 100 ms for a message
   *
   * A thread waiting in send() or receive() is BLOCKED and uses no CPU time
   * until another thread or interrupt makes room or delivers an item.
   */
  template <class T, unsigned int N> class Queue : public QueueBase {
    static_assert(N > 0, "Queue capacity must be at least 1");
    static_assert(std::is_trivially_copyable<T>::value,
                  "Queue items must be trivially copyable; send a pointer instead");
  private:
    alignas(T) uint8_t storage[N * sizeof(T)];
  public:
    Queue() : QueueBase(storage, sizeof(T), N) {}
    bool send(const T &item, unsigned int timeout_ms = 0) { return QueueBase::send(&item, timeout_ms); }
    bool trySend(const T &item) { return QueueBase::trySend(&item); }
    bool receive(T &item, unsigned int timeout_ms = 0) { return QueueBase::receive(&item, timeout_ms); }
    bool tryReceive(T &item) { return QueueBase::tryReceive(&item); }
    bool peek(T &item) { return QueueBase::peek(&item); }
  };

  class Scope {
  private:
    Mutex *r;
  public:
    Scope(Mutex& m) { r = &m; r->lock(); }
    ~Scope() { r->unlock(); }
  };

  class Suspend {
  private:
    int save_state;
  public:
    Suspend();      // Stop threads and save thread state
    ~Suspend();     // Restore saved state
  };

  template <class C> class GrabTemp {
    private:
      Mutex *lkp;
    public:
      C *me;
      GrabTemp(C *obj, Mutex *lk) { me = obj; lkp=lk; lkp->lock(); }
      ~GrabTemp() { lkp->unlock(); }
      C &get() { return *me; }
  };

  template <class T> class Grab {
    private:
      Mutex lk;
      T *me;
    public:
      Grab(T &t) { me = &t; }
      GrabTemp<T> grab() { return GrabTemp<T>(me, &lk); }
      operator T&() { return grab().get(); }
      T *operator->() { return grab().me; }
      Mutex &getLock() { return lk; }
  };

#define ThreadWrap(OLDOBJ, NEWOBJ) Threads::Grab<decltype(OLDOBJ)> NEWOBJ(OLDOBJ);
#define ThreadClone(NEWOBJ) (NEWOBJ.grab().get())

};


extern Threads threads;

/*
 * Rudimentary compliance to C++11 class
 *
 * See http://www.cplusplus.com/reference/thread/thread/
 *
 * Example:
 * int x;
 * void thread_func() { x++; }
 * int main() {
 *   std::thread(thread_func);
 * }
 *
 */
namespace std {
  class thread {
  private:
    int id;          // internal thread id
    int destroy;     // flag to kill thread on instance destruction
  public:
    // By casting all (args...) to (void*), if there are more than one args, the compiler
    // will fail to find a matching function. This fancy template just allows any kind of
    // function to match.
    template <class F, class ...Args> explicit thread(F&& f, Args&&... args) {
      id = threads.addThread((ThreadFunction)f, (void*)args...);
      destroy = 1;
    }
    // If thread has not been detached when destructor called, then thread must end
    ~thread() {
      if (destroy) threads.kill(id);
    }
    // Threads are joinable until detached per definition, but in this implementation
    // that's not so. We emulate expected behavior anyway.
    bool joinable() { return destroy==1; }
    // Once detach() is called, thread runs until it terminates; otherwise it terminates
    // when destructor called.
    void detach() { destroy = 0; }
    // In theory, the thread merges with the running thread; if we just wait until
    // termination, it's basically the same thing except it's slower because
    // there are two threads running instead of one. Close enough.
    void join() { threads.wait(id); }
    // Get the unique thread id.
    int get_id() { return id; }
  };

  class mutex {
    private:
      Threads::Mutex mx;
    public:
      void lock() { mx.lock(); }
      bool try_lock() { return mx.try_lock(); }
      void unlock() { mx.unlock(); }
  };

  template <class cMutex> class lock_guard {
    private:
      cMutex *r;
    public:
      explicit lock_guard(cMutex& m) { r = &m; r->lock(); }
      ~lock_guard() { r->unlock(); }
  };
}

#endif
