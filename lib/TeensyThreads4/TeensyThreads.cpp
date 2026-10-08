/*
 * Threads.cpp - Library for threading on the Teensy.
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
 */
#include "TeensyThreads.h"
#include <Arduino.h>
#include <string.h>
#include <new>

Threads threads;

unsigned int time_start;
unsigned int time_end;

#define __flush_cpu() __asm__ volatile("DMB");

// These variables are used by the assembly context_switch() function.
// They are copies or pointers to data in Threads and ThreadInfo
// and put here seperately in order to simplify the code.
extern "C" {
  __attribute__((used)) int currentUseSystick;      // using Systick vs PIT/GPT
  __attribute__((used)) int currentActive;          // state of the system (first, start, stop)
  __attribute__((used)) int currentCount;
  __attribute__((used)) ThreadInfo *currentThread;  // the thread currently running
  __attribute__((used)) void *currentSave;
  __attribute__((used)) int currentMSP;             // Stack pointers to save
  __attribute__((used)) void *currentSP;
  __attribute__((used)) void loadNextThread() {
    threads.getNextThread();
  }
}

const int overflow_stack_size = 8;
const uint32_t thread_marker = 0xDEADDEAD;

extern "C" void stack_overflow_default_isr() { 
  currentThread->flags = Threads::ENDED;
}
extern "C" void stack_overflow_isr(void)       __attribute__ ((weak, alias("stack_overflow_default_isr")));

extern unsigned long _estack;   // the main thread 0 stack

// static void threads_svcall_isr(void);
// static void threads_systick_isr(void);

IsrFunction Threads::save_systick_isr;
IsrFunction Threads::save_svcall_isr;

/*
 * Handle the SVC instruction used by yield(). Note that this function is
 * "naked" meaning it does not save it's registers on the stack. This is so
 * we can preserve the stack of the caller.
 */
void __attribute((naked, noinline)) threads_svcall_isr(void)
{
  if (Threads::save_svcall_isr) {
    asm volatile("push {r0-r4,lr}");
    (*Threads::save_svcall_isr)();
    asm volatile("pop {r0-r4,lr}");
  }

  // Get the right stack so we can extract the PC (next instruction)
  // and then see the SVC calling instruction number
  __asm volatile("TST lr, #4 \n"
                 "ITE EQ \n"
                 "MRSEQ r0, msp \n"
                 "MRSNE r0, psp \n");
  register unsigned int *rsp __asm("r0");
  unsigned int svc = ((uint8_t*)rsp[6])[-2];
  if (svc == Threads::SVC_NUMBER) {
    __asm volatile("b context_switch_direct");
  }
  else if (svc == Threads::SVC_NUMBER_ACTIVE) {
    currentActive = Threads::STARTED;
    __asm volatile("b context_switch_direct_active");
  }
  __asm volatile("bx lr");
}

/*
 * 
 * Teensy 4:
 * Use unused GPT timers for context switching
 */

extern "C" void unused_interrupt_vector(void);

static void __attribute((naked, noinline)) gpt1_isr() {
  GPT1_SR |= GPT_SR_OF1;  // clear set bit
  __asm volatile ("dsb"); // see github bug #20 by manitou48
  __asm volatile("b context_switch");
}

static void __attribute((naked, noinline)) gpt2_isr() {
  GPT2_SR |= GPT_SR_OF1;  // clear set bit
  __asm volatile ("dsb"); // see github bug #20 by manitou48
  __asm volatile("b context_switch");
}

bool gtp1_init(unsigned int microseconds)
{
  // Initialization code derived from @manitou48.
  // See https://github.com/manitou48/teensy4/blob/master/gpt_isr.ino
  // See https://forum.pjrc.com/threads/54265-Teensy-4-testing-mbed-NXP-MXRT1050-EVKB-(600-Mhz-M7)?p=193217&viewfull=1#post193217

  // keep track of which GPT timer we are using
  static int gpt_number = 0;

  // not configured yet, so find an inactive GPT timer
  if (gpt_number == 0) {
    if (! NVIC_IS_ENABLED(IRQ_GPT1)) {
      attachInterruptVector(IRQ_GPT1, &gpt1_isr);
      NVIC_SET_PRIORITY(IRQ_GPT1, 255);
      NVIC_ENABLE_IRQ(IRQ_GPT1);
      gpt_number = 1;
    }
    else if (! NVIC_IS_ENABLED(IRQ_GPT2)) {
      attachInterruptVector(IRQ_GPT2, &gpt2_isr);
      NVIC_SET_PRIORITY(IRQ_GPT2, 255);
      NVIC_ENABLE_IRQ(IRQ_GPT2);
      gpt_number = 2;
    }
    else {
      // if neither timer is free, we fail
      return false;
    }
  }

  switch (gpt_number) {
    case 1:
      CCM_CCGR1 |= CCM_CCGR1_GPT1_BUS(CCM_CCGR_ON) ;  // enable GPT1 module
      GPT1_CR = 0;                   // disable timer
      GPT1_PR = 23;                  // prescale: divide by 24 so 1 tick = 1 microsecond at 24MHz
      GPT1_OCR1 = microseconds - 1;  // compare value
      GPT1_SR = 0x3F;                // clear all prior status
      GPT1_IR = GPT_IR_OF1IE;        // use first timer
      GPT1_CR = GPT_CR_EN | GPT_CR_CLKSRC(1) ; // set to peripheral clock (24MHz)
      break;
    case 2:
      CCM_CCGR1 |= CCM_CCGR1_GPT1_BUS(CCM_CCGR_ON) ;  // enable GPT1 module
      GPT2_CR = 0;                   // disable timer
      GPT2_PR = 23;                  // prescale: divide by 24 so 1 tick = 1 microsecond at 24MHz
      GPT2_OCR1 = microseconds - 1;  // compare value
      GPT2_SR = 0x3F;                // clear all prior status
      GPT2_IR = GPT_IR_OF1IE;        // use first timer
      GPT2_CR = GPT_CR_EN | GPT_CR_CLKSRC(1) ; // set to peripheral clock (24MHz)
      break;
    default:
      return false;
  }

  return true;
}

/*************************************************/
/**\name UTILITIES FUNCTIONS                     */
/*************************************************/
/**
 * \brief Convert thead state to printable string
 */
char * _util_state_2_string(int state){
    static char _state[Threads::UTIL_STATE_NAME_DESCRIPTION_LENGTH];
    memset(_state, 0, sizeof(_state));

    switch (state)
    {
    case 0:
        sprintf(_state, "EMPTY");
        break;
    case 1:
        sprintf(_state, "RUNNING");
        break;
    case 2:
        sprintf(_state, "ENDED");
        break;
    case 3:
        sprintf(_state, "ENDING");
        break;
    case 4:
        sprintf(_state, "SUSPENDED");
        break;
    case 5:
        sprintf(_state, "SLEEPING");
        break;
    case 6:
        sprintf(_state, "BLOCKED");
        break;
    default:
        sprintf(_state, "%d", state);
        break;
    }
    
    return _state;
}

/*************************************************/
/**\name CLASS THREAD                            */
/*************************************************/
Threads::Threads() : current_thread(0), thread_count(0), thread_error(0) {
  // thread 0 is the head of the list and is always running
  threadp = new ThreadInfo();
  threadp->id = 0;
  threadp->next = NULL;

  // initialize context_switch() globals from thread 0, which is MSP and always running
  currentThread = threadp;        // thread 0 is active
  currentSave = &threadp->save;
  currentMSP = 1;
  currentSP = 0;
  currentCount = Threads::DEFAULT_TICKS;
  currentActive = FIRST_RUN;
  threadp->flags = RUNNING;
  threadp->ticks = DEFAULT_TICKS;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
  threadp->stack = (uint8_t*)&_estack - DEFAULT_STACK0_SIZE;
#pragma GCC diagnostic pop
  threadp->stack_size = DEFAULT_STACK0_SIZE;
  setStackMarker(threadp->stack);

  cpu_switch_start = ARM_DWT_CYCCNT;
  cpu_window_start = cpu_switch_start;

  // commandeer SVCall & use GTP1 Interrupt
  save_svcall_isr = _VectorsRam[11];
  if (save_svcall_isr == unused_interrupt_vector) save_svcall_isr = 0;
  _VectorsRam[11] = threads_svcall_isr;

  currentUseSystick = 0; // disable Systick calls
  gtp1_init(1000);       // tick every millisecond
}

/*
 * start() - Begin threading
 */
int Threads::start(int prev_state) {
  __disable_irq();
  int old_state = currentActive;
  if (prev_state == -1) prev_state = STARTED;
  currentActive = prev_state;
  __enable_irq();
  return old_state;
}

/*
 * stop() - Stop threading, even if active.
 *
 * If threads have already started, this should be called sparingly
 * because it could destabalize the system if thread 0 is stopped.
 */
int Threads::stop() {
  __disable_irq();
  int old_state = currentActive;
  currentActive = STOPPED;
  __enable_irq();
  return old_state;
}

/*
 * getNextThread() - Find next running thread
 *
 * This will also set the context_switcher() state variables
 */
void Threads::getNextThread() {

  // Keep track of the number of cycles expended by each thread.
  // See @dfragster: https://forum.pjrc.com/threads/41504-Teensy-3-x-multithreading-library-first-release?p=213086#post213086
  uint32_t now = ARM_DWT_CYCCNT;
  uint32_t elapsed = now - cpu_switch_start;
  currentThread->cyclesAccum += elapsed;
  currentThread->cyclesWindow += elapsed;
  cpu_switch_start = now;

  // At the end of each window, latch every thread's cycle count so
  // getCPUUsage() can report a consistent snapshot
  uint32_t window_length = now - cpu_window_start;
  if (window_length >= cpu_window_ms * (F_CPU_ACTUAL / 1000)) {
    for (ThreadInfo *tp = threadp; tp != NULL; tp = tp->next) {
      // Fold the window into the float total here rather than on every
      // switch: adding whole windows keeps float rounding error small
      tp->secondsAccum += (float)tp->cyclesWindow / (float)F_CPU_ACTUAL;
      tp->cyclesLastWindow = tp->cyclesWindow;
      tp->cyclesWindow = 0;
    }
    cpu_last_window_cycles = window_length;
    cpu_window_start = now;
  }

  // First, save the currentSP set by context_switch. Thread 0 runs on MSP,
  // which context_switch does not save, so read it directly (we are in an
  // interrupt on the MSP, so this includes the exception frame).
  if (currentThread == threadp) {
    void *msp;
    __asm volatile("mrs %0, msp" : "=r"(msp));
    currentThread->sp = msp;
  }
  else {
    currentThread->sp = currentSP;
  }

  // did we overflow the stack (don't check thread 0)? Either the stack pointer
  // is at the bottom of the stack now, or the marker at the bottom of the stack
  // has been overwritten since the last switch.
  // allow an extra 8 bytes for a call to the ISR and one additional call or variable
  if (current_thread && ((uint8_t*)currentThread->sp - currentThread->stack <= overflow_stack_size
                         || *(uint32_t*)currentThread->stack != thread_marker)) {
    stack_overflow_isr();
  }

  // Find the next running thread, waking any sleeping thread (or blocked
  // thread with a timeout) whose time is up. Threads before the current one
  // are checked on the next pass of the list.
  uint32_t now_ms = millis();
  ThreadInfo *tp = currentThread->next;
  while (tp != NULL) {
    int flags = tp->flags;
    if ((flags == SLEEPING || (flags == BLOCKED && tp->wait_timed))
        && (int32_t)(now_ms - tp->wake_time) >= 0) {
      tp->flags = RUNNING;
    }
    if (tp->flags == RUNNING) break;
    tp = tp->next;
  }
  if (tp == NULL) tp = threadp; // end of list; thread 0 is MSP and always active

  current_thread = tp->id;
  currentCount = tp->ticks;

  currentThread = tp;
  currentSave = &tp->save;
  currentMSP = (tp == threadp ? 1 : 0);
  currentSP = tp->sp;
}

/*
 * getThreadInfo() - Find the thread with the given id
 *
 * Returns NULL if no such thread exists.
 */
ThreadInfo *Threads::getThreadInfo(int id) {
  if (id < 0) return NULL;
  for (ThreadInfo *tp = threadp; tp != NULL; tp = tp->next) {
    if (tp->id == id) return tp;
    if (tp->id > id) break; // list is ordered by id
  }
  return NULL;
}

/*
 * Store the PIT timer flag register for use in assembly
 */
volatile uint32_t *context_timer_flag;

/*
 * Stop using the SysTick interrupt and start using
 * the IntervalTimer timer. The parameter is the number of microseconds
 * for each tick.
 */
int Threads::setMicroTimer(int tick_microseconds)
{
  gtp1_init(tick_microseconds);

  return 1;
}

/*
 * Set each time slice to be 'microseconds' long
 */
int Threads::setSliceMicros(int microseconds)
{
  setMicroTimer(microseconds);
  setDefaultTimeSlice(1);
  return 1;
}

/*
 * Set each time slice to be 'milliseconds' long
 */
int Threads::setSliceMillis(int milliseconds)
{
  if (currentUseSystick) {
    setDefaultTimeSlice(milliseconds);
  }
  else {
    // if we're using the PIT, we should probably really disable it and
    // re-establish the systick timer; but this is easier for now
    setSliceMicros(milliseconds * 1000);
  }
  return 1;
}

/*
 * del_process() - This is called when the task returns
 *
 * Turns thread off. Thread continues running until next call to
 * context_switch() at which point it all stops. The while(1) statement
 * just stalls until such time.
 */
void Threads::del_process(void)
{
  int old_state = threads.stop();
  ThreadInfo *me = currentThread;
  // The stack can't be freed here since we are still running on it. A stack
  // the library allocated stays with this node and is reused by addThread().
  threads.thread_count--;
  me->flags = ENDED; //clear the flags so thread can stop and be reused
  threads.start(old_state);
  while(1); // just in case, keep working until context change when execution will not return to this thread
}

/*
 * Set a marker at memory so we can detect memory overruns
 */

void Threads::setStackMarker(void *stack)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
  uint32_t *m = (uint32_t*)stack;
  *m = thread_marker;
#pragma GCC diagnostic pop
}

/*
 * Users call this function to see if stack has been corrupted
 */
int Threads::testStackMarkers(int *threadid)
{
  for (ThreadInfo *tp = threadp; tp != NULL; tp = tp->next) {
    if (tp->flags != ENDED && tp->flags != EMPTY) {
      uint32_t *m = (uint32_t*)tp->stack;
      if (*m != thread_marker) {
        if (threadid) *threadid = tp->id;
        return -1;
      }
    }
  }
  return 0;
}

/*
 * Initializes a thread's stack. Called when thread is created
 */
void *Threads::loadstack(ThreadFunction p, void * arg, void *stackaddr, int stack_size)
{
  interrupt_stack_t * process_frame = (interrupt_stack_t *)((uint8_t*)stackaddr + stack_size - sizeof(interrupt_stack_t) - overflow_stack_size);
  process_frame->r0 = (uint32_t)arg;
  process_frame->r1 = 0;
  process_frame->r2 = 0;
  process_frame->r3 = 0;
  process_frame->r12 = 0;
  process_frame->lr = (uint32_t)Threads::del_process;
  process_frame->pc = ((uint32_t)p);
  process_frame->xpsr = 0x1000000;
  uint8_t *ret = (uint8_t*)process_frame;
  // ret -= sizeof(software_stack_t); // uncomment this if we are saving R4-R11 to the stack
  return (void*)ret;
}

/*
 * Allocate a new ThreadInfo. If stack_size > 0, a stack of that size is
 * allocated in the same block, with the ThreadInfo directly above it so a
 * stack overflow runs away from the thread's own state.
 * Returns NULL if out of memory.
 */
ThreadInfo *Threads::newThreadInfo(int stack_size)
{
  if (stack_size <= 0) return new (std::nothrow) ThreadInfo();
  // keep the top of the stack, and so the ThreadInfo, 8 byte aligned
  stack_size = (stack_size + 7) & ~7;
  uint8_t *block = new (std::nothrow) uint8_t[stack_size + sizeof(ThreadInfo)];
  if (block == NULL) return NULL;
  ThreadInfo *tp = new (block + stack_size) ThreadInfo();
  tp->block_stack = block;
  tp->block_stack_size = stack_size;
  return tp;
}

/*
 * Add a new thread to the queue.
 *    add_thread(fund, arg)
 *
 *    fund : is a function pointer. The function prototype is:
 *           void *func(void *param)
 *    arg  : is a void pointer that is passed as the first parameter
 *           of the function. In the example above, arg is passed
 *           as param.
 *    stack_size : the size of the buffer pointed to by stack. If
 *           it is 0, then "stack" must also be 0. If so, the function
 *           will allocate the default stack size of the heap using new().
 *    stack : pointer to new data stack of size stack_size. If this is 0,
 *           then it will allocate a stack on the heap using new() of size
 *           stack_size. If stack_size is 0, a default size will be used.
 *    return: an integer ID to be used for other calls
 */
int Threads::addThread(ThreadFunction p, void * arg, int stack_size, void *stack)
{
  int old_state = stop();
  if (stack_size == -1) stack_size = DEFAULT_STACK_SIZE;
  if (stack == 0 && stack_size <= 0) stack_size = DEFAULT_STACK_SIZE;

  // Reuse a thread that has ended, if any; otherwise append a new one.
  // Nodes are never freed, so their stacks are reused along with them; if the
  // library is to allocate the stack, only reuse a node whose stack is big enough.
  ThreadInfo *tp = NULL;
  ThreadInfo *last = threadp;
  for (ThreadInfo *t = threadp->next; t != NULL; t = t->next) {
    if ((t->flags == ENDED || t->flags == EMPTY)
        && (stack != 0 || t->block_stack_size >= stack_size)) {
      tp = t;
      break;
    }
    last = t;
  }
  while (last->next != NULL) last = last->next;
  bool is_new = (tp == NULL);
  if (is_new) {
    tp = newThreadInfo(stack == 0 ? stack_size : 0);
    if (tp == NULL) {
      if (old_state == STARTED) start();
      return -1;
    }
    tp->id = last->id + 1;
    tp->next = NULL;
  }

  if (stack==0) {
    // use all of the node's stack, which may be larger than requested
    stack = tp->block_stack;
    stack_size = tp->block_stack_size;
    tp->my_stack = 1;
  }
  else {
    tp->my_stack = 0;
  }
  setStackMarker(stack);
  tp->stack = (uint8_t*)stack;
  tp->stack_size = stack_size;
  void *psp = loadstack(p, arg, tp->stack, tp->stack_size);
  tp->sp = psp;
  tp->ticks = DEFAULT_TICKS;
  tp->save.lr = 0xFFFFFFF9;

  tp->cyclesAccum = 0;
  tp->cyclesWindow = 0;
  tp->cyclesLastWindow = 0;
  tp->secondsAccum = 0;
  tp->generation++; // lets wait() tell this thread apart from an earlier one with the same id
  tp->flags = RUNNING;

  // Link in only once fully initialized
  if (is_new) last->next = tp;

  currentActive = old_state;
  thread_count++;
  if (old_state == STARTED || old_state == FIRST_RUN) start();
  return tp->id;
}

int Threads::getState(int id)
{
  ThreadInfo *tp = getThreadInfo(id);
  if (tp == NULL) return EMPTY;
  return tp->flags;
}

int Threads::setState(int id, int state)
{
  ThreadInfo *tp = getThreadInfo(id);
  if (tp == NULL) return -1;
  tp->flags = state;
  return state;
}

int Threads::wait(int id, unsigned int timeout_ms)
{
  ThreadInfo *tp = getThreadInfo(id);
  if (tp == NULL) return id;
  if (tp == currentThread) return -1; // a thread can never see itself end
  // If the thread ends and its id is reused by addThread() before we look
  // again, the generation changes even though flags may read RUNNING.
  uint32_t generation = tp->generation;
  unsigned int start = millis();
  // need to store state in temp volatile memory for optimizer.
  // "while (thread[id].flags != ENDED)" will be optimized away
  volatile int state;
  while (1) {
    if (timeout_ms != 0 && millis() - start > timeout_ms) return -1;
    state = tp->flags;
    if (tp->generation != generation) break;
    // SUSPENDED and SLEEPING threads have not ended, so keep waiting
    if (state == ENDED || state == EMPTY) break;
    yield();
  }
  return id;
}

int Threads::kill(int id)
{
  ThreadInfo *tp = getThreadInfo(id);
  if (tp == NULL) return -1;
  tp->flags = ENDED;
  return id;
}

int Threads::suspend(int id)
{
  ThreadInfo *tp = getThreadInfo(id);
  if (tp == NULL) return -1;
  tp->flags = SUSPENDED;
  return id;
}

int Threads::restart(int id)
{
  ThreadInfo *tp = getThreadInfo(id);
  if (tp == NULL) return -1;
  tp->flags = RUNNING;
  return id;
}

void Threads::setTimeSlice(int id, unsigned int ticks)
{
  ThreadInfo *tp = getThreadInfo(id);
  if (tp == NULL) return;
  tp->ticks = ticks - 1;
}

void Threads::setDefaultTimeSlice(unsigned int ticks)
{
  DEFAULT_TICKS = ticks - 1;
}

void Threads::setDefaultStackSize(unsigned int bytes_size)
{
  DEFAULT_STACK_SIZE = bytes_size;
}

void Threads::yield() {
  __asm volatile("svc %0" : : "i"(Threads::SVC_NUMBER));
}

void Threads::yield_and_start() {
  __asm volatile("svc %0" : : "i"(Threads::SVC_NUMBER_ACTIVE));
}

void Threads::delay(int millisecond) {
  int mx = millis();
  while((int)millis() - mx < millisecond) yield();
}

/*
 * Experimental code for putting CPU into sleep mode during delays
 */

void Threads::setSleepCallback(ThreadFunctionSleep callback) 
{
  enter_sleep_callback = callback;
}

void Threads::delay_us(int microsecond){
  int mx = micros();
  while ((int)micros() - mx < microsecond) yield();
}

void Threads::idle() {
  if (enter_sleep_callback==NULL) return;

  __disable_irq();
  // find the soonest wake-up time among sleeping threads
  uint32_t now = millis();
  bool any_sleeping = false;
  int32_t sleep_ms = 0;
  for (ThreadInfo *tp = threadp->next; tp != NULL; tp = tp->next) {
    if (tp->flags != SLEEPING && !(tp->flags == BLOCKED && tp->wait_timed)) continue;
    int32_t remaining = (int32_t)(tp->wake_time - now);
    if (!any_sleeping || remaining < sleep_ms) sleep_ms = remaining;
    any_sleeping = true;
  }

  if (any_sleeping && sleep_ms > 0) {
    int time_spent_asleep = enter_sleep_callback(sleep_ms);
    // Deep sleep modes stop the systick, so millis() may not have advanced
    // while asleep. Pull wake times earlier by whatever millis() missed.
    int32_t missed = time_spent_asleep - (int32_t)(millis() - now);
    if (missed > 0) {
      for (ThreadInfo *tp = threadp->next; tp != NULL; tp = tp->next) {
        if (tp->flags == SLEEPING || tp->flags == BLOCKED) tp->wake_time -= missed;
      }
    }
  }
  __enable_irq();
  yield(); // the scheduler wakes any thread whose time is up
}

void Threads::sleep(int ms) {
  ThreadInfo *tp = currentThread;
  if (ms <= 0) {
    yield();
    return;
  }
  __disable_irq();
  if (tp->flags != RUNNING) {
    __enable_irq();
    return;
  }
  tp->wake_time = millis() + ms;
  tp->flags = SLEEPING;
  __enable_irq();

  // getNextThread() wakes the thread when its time is up; until then it is
  // not scheduled. Thread 0 is always scheduled, and yield() returns at once
  // if threading is stopped, so also check the time here.
  while (tp->flags == SLEEPING) {
    __disable_irq();
    if (tp->flags == SLEEPING && (int32_t)(millis() - tp->wake_time) >= 0) {
      tp->flags = RUNNING;
    }
    __enable_irq();
    if (tp->flags == SLEEPING) yield();
  }
}

/* End of experimental code */

/*
 * block() - Mark the current thread BLOCKED waiting on obj
 *
 * Call with interrupts disabled, in the same critical section that found
 * the thread must wait, so a wake() cannot be missed. Then re-enable
 * interrupts and call waitWhileBlocked(). If timeout_ms is 0, the thread
 * waits until woken.
 */
void Threads::block(const void *obj, unsigned int timeout_ms) {
  ThreadInfo *tp = currentThread;
  tp->wait_obj = obj;
  tp->wait_timed = (timeout_ms != 0);
  tp->wake_time = millis() + timeout_ms;
  tp->flags = BLOCKED;
}

/*
 * waitWhileBlocked() - Give up the CPU until wake() or the timeout
 *
 * Returns once the thread is no longer BLOCKED. Callers must recheck their
 * condition since the thread may also have been woken by a timeout,
 * restart(), or another thread consuming what it was waiting for.
 */
void Threads::waitWhileBlocked() {
  ThreadInfo *tp = currentThread;
  // getNextThread() does not schedule a BLOCKED thread, but thread 0 is
  // always scheduled, and yield() returns at once if threading is stopped,
  // so also check the timeout here (as sleep() does).
  while (tp->flags == BLOCKED) {
    __disable_irq();
    if (tp->flags == BLOCKED && tp->wait_timed
        && (int32_t)(millis() - tp->wake_time) >= 0) {
      tp->flags = RUNNING;
    }
    __enable_irq();
    if (tp->flags == BLOCKED) yield();
  }
  tp->wait_obj = NULL;
}

/*
 * wake() - Make every thread BLOCKED on obj runnable again
 *
 * Call with interrupts disabled. Safe to call from an interrupt.
 */
void Threads::wake(const void *obj) {
  for (ThreadInfo *tp = threadp; tp != NULL; tp = tp->next) {
    if (tp->flags == BLOCKED && tp->wait_obj == obj) {
      tp->wait_obj = NULL;
      tp->flags = RUNNING;
    }
  }
}

int Threads::id() {
  volatile int ret;
  __disable_irq();
  ret = current_thread;
  __enable_irq();
  return ret;
}

/*
 * Current stack pointer of a thread: live for thread 0 (MSP) and the
 * running thread (PSP), otherwise as saved at its last context switch
 */
uint8_t *Threads::stackPointer(ThreadInfo *tp) {
  void *sp;
  if (tp == threadp) {
    __asm volatile("mrs %0, msp" : "=r"(sp));
  }
  else if (tp == currentThread) {
    __asm volatile("mrs %0, psp" : "=r"(sp));
  }
  else {
    sp = tp->sp;
  }
  return (uint8_t*)sp;
}

int Threads::getStackUsed(int id) {
  ThreadInfo *tp = getThreadInfo(id);
  if (tp == NULL) return 0;
  return tp->stack + tp->stack_size - stackPointer(tp);
}

int Threads::getStackRemaining(int id) {
  ThreadInfo *tp = getThreadInfo(id);
  if (tp == NULL) return 0;
  return stackPointer(tp) - tp->stack;
}

char *Threads::threadsInfo(void)
{
  static char _buffer[Threads::UTIL_TRHEADS_BUFFER_LENGTH];
  unsigned int _buffer_cursor = 0;
  _buffer_cursor = sprintf(_buffer, "_____\n");
  for (ThreadInfo *tp = threadp; tp != NULL; tp = tp->next)
  {
    // each line is well under 160 bytes; stop before overflowing the buffer
    if (_buffer_cursor + 160 > sizeof(_buffer)) break;
    _buffer_cursor += sprintf(_buffer + _buffer_cursor, "%d:", tp->id);
    _buffer_cursor += sprintf(_buffer + _buffer_cursor, "Stack size:%d|",
                              tp->stack_size);
    _buffer_cursor += sprintf(_buffer + _buffer_cursor, "Used:%d|Remains:%d|",
                              getStackUsed(tp->id),
                              getStackRemaining(tp->id));
    char *_thread_state = _util_state_2_string(tp->flags);
    _buffer_cursor += sprintf(_buffer + _buffer_cursor, "State:%s|",
                              _thread_state);
    int cpu_tenths = (int)(getCPUUsage(tp->id) * 10.0f + 0.5f);
    unsigned long run_ms = (unsigned long)(getSecondsUsed(tp->id) * 1000.0f + 0.5f);
    _buffer_cursor += sprintf(_buffer + _buffer_cursor, "CPU:%d.%d%%|cycles:%lu|time:%lu.%03lus\n",
                              cpu_tenths / 10, cpu_tenths % 10,
                              tp->cyclesAccum, run_ms / 1000, run_ms % 1000);
  }
  return _buffer;
}

unsigned long Threads::getCyclesUsed(int id) {
  ThreadInfo *tp = getThreadInfo(id);
  if (tp == NULL) return 0;
  __disable_irq();
  unsigned long ret = tp->cyclesAccum;
  __enable_irq();
  return ret;
}

float Threads::getSecondsUsed(int id) {
  ThreadInfo *tp = getThreadInfo(id);
  if (tp == NULL) return 0.0f;
  __disable_irq();
  float seconds = tp->secondsAccum;
  uint32_t pending = tp->cyclesWindow;
  __enable_irq();
  return seconds + (float)pending / (float)F_CPU_ACTUAL;
}

float Threads::getCPUUsage(int id) {
  ThreadInfo *tp = getThreadInfo(id);
  if (tp == NULL) return 0.0f;
  __disable_irq();
  uint32_t used = tp->cyclesLastWindow;
  uint32_t total = cpu_last_window_cycles;
  __enable_irq();
  if (total == 0) return 0.0f; // no window completed yet
  return used * 100.0f / total;
}

void Threads::setCPUUsageWindow(unsigned int ms) {
  if (ms == 0) ms = 1;
  if (ms > MAX_CPU_WINDOW_MS) ms = MAX_CPU_WINDOW_MS;
  __disable_irq();
  cpu_window_ms = ms;
  __enable_irq();
}

/*
 * On creation, stop threading and save state
 */
Threads::Suspend::Suspend() {
  __disable_irq();
  save_state = currentActive;
  currentActive = 0;
  __enable_irq();
}

/*
 * On destruction, restore threading state
 */
Threads::Suspend::~Suspend() {
  __disable_irq();
  currentActive = save_state;
  __enable_irq();
}

int Threads::Mutex::getState() {
  int p = threads.stop();
  int ret = state;
  threads.start(p);
  return ret;
}

int __attribute__ ((noinline)) Threads::Mutex::lock(unsigned int timeout_ms) {
  if (try_lock()) return 1; // we're good, so avoid more checks

  uint32_t start = systick_millis_count;
  while (1) {
    if (try_lock()) return 1;
    if (timeout_ms && (systick_millis_count - start > timeout_ms)) return 0;
    if (waitthread==-1) { // can hold 1 thread suspend until unlock
      int p = threads.stop();
      waitthread = threads.current_thread;
      waitcount = currentCount;
      threads.suspend(waitthread);
      threads.start(p);
    }
    threads.yield();
  }
  __flush_cpu();
  return 0;
}

int Threads::Mutex::try_lock() {
  int p = threads.stop();
  if (state == 0) {
    state = 1;
    threads.start(p);
    return 1;
  }
  threads.start(p);
  return 0;
}

int __attribute__ ((noinline)) Threads::Mutex::unlock() {
  int p = threads.stop();
  if (state==1) {
    state = 0;
    if (waitthread >= 0) { // reanimate a suspended thread waiting for unlock
      threads.restart(waitthread);
      waitthread = -1;
      __flush_cpu();
      threads.yield_and_start();
      return 1;
    }
  }
  __flush_cpu();
  threads.start(p);
  return 1;
}


/*
 * Queue
 *
 * The queue is protected by disabling interrupts rather than with a Mutex,
 * so it can also be used from interrupt handlers. PRIMASK is saved and
 * restored so calls made with interrupts already disabled leave them that way.
 */

static inline uint32_t queue_irq_save() {
  uint32_t primask;
  __asm volatile("mrs %0, primask\n"
                 "cpsid i" : "=r"(primask) : : "memory");
  return primask;
}

static inline void queue_irq_restore(uint32_t primask) {
  __asm volatile("msr primask, %0" : : "r"(primask) : "memory");
}

// A thread may only wait if it can yield: not inside an interrupt, and
// not with interrupts disabled (an SVC with PRIMASK set is a HardFault)
static inline bool queue_can_wait(uint32_t primask) {
  uint32_t ipsr;
  __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
  return ipsr == 0 && primask == 0;
}

Threads::QueueBase::QueueBase(uint8_t *buffer, size_t item_size, unsigned int capacity)
  : buf(buffer), item_size(item_size), cap(capacity) {
}

bool Threads::QueueBase::put(const void *item, unsigned int timeout_ms, bool can_wait) {
  uint32_t start = millis();
  while (1) {
    uint32_t primask = queue_irq_save();
    if (used < cap) {
      unsigned int tail = head + used;
      if (tail >= cap) tail -= cap;
      memcpy(buf + tail * item_size, item, item_size);
      used = used + 1;
      threads.wake(&readers);
      queue_irq_restore(primask);
      return true;
    }
    if (!can_wait || !queue_can_wait(primask)) {
      queue_irq_restore(primask);
      return false;
    }
    unsigned int remaining = 0;
    if (timeout_ms) {
      uint32_t elapsed = millis() - start;
      if (elapsed >= timeout_ms) {
        queue_irq_restore(primask);
        return false;
      }
      remaining = timeout_ms - elapsed;
    }
    threads.block(&writers, remaining);
    queue_irq_restore(primask);
    threads.waitWhileBlocked();
  }
}

bool Threads::QueueBase::get(void *item, unsigned int timeout_ms, bool can_wait) {
  uint32_t start = millis();
  while (1) {
    uint32_t primask = queue_irq_save();
    if (used > 0) {
      memcpy(item, buf + head * item_size, item_size);
      head = (head + 1 >= cap) ? 0 : head + 1;
      used = used - 1;
      threads.wake(&writers);
      queue_irq_restore(primask);
      return true;
    }
    if (!can_wait || !queue_can_wait(primask)) {
      queue_irq_restore(primask);
      return false;
    }
    unsigned int remaining = 0;
    if (timeout_ms) {
      uint32_t elapsed = millis() - start;
      if (elapsed >= timeout_ms) {
        queue_irq_restore(primask);
        return false;
      }
      remaining = timeout_ms - elapsed;
    }
    threads.block(&readers, remaining);
    queue_irq_restore(primask);
    threads.waitWhileBlocked();
  }
}

bool Threads::QueueBase::send(const void *item, unsigned int timeout_ms) {
  return put(item, timeout_ms, true);
}

bool Threads::QueueBase::trySend(const void *item) {
  return put(item, 0, false);
}

bool Threads::QueueBase::receive(void *item, unsigned int timeout_ms) {
  return get(item, timeout_ms, true);
}

bool Threads::QueueBase::tryReceive(void *item) {
  return get(item, 0, false);
}

bool Threads::QueueBase::peek(void *item) {
  uint32_t primask = queue_irq_save();
  bool ok = used > 0;
  if (ok) memcpy(item, buf + head * item_size, item_size);
  queue_irq_restore(primask);
  return ok;
}

void Threads::QueueBase::clear() {
  uint32_t primask = queue_irq_save();
  head = 0;
  used = 0;
  threads.wake(&writers);
  queue_irq_restore(primask);
}

unsigned int Threads::QueueBase::count() {
  return used;
}

unsigned int Threads::QueueBase::space() {
  uint32_t primask = queue_irq_save();
  unsigned int ret = cap - used;
  queue_irq_restore(primask);
  return ret;
}
