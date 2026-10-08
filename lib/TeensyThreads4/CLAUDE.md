# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

TeensyThreads: an Arduino library giving preemptive threads (plus mutexes, a
`std::thread`/`std::mutex` shim, and `Threads::Queue` message queues) on
Teensy 4.0 / 4.1 / MicroMod only (i.MX RT1062, Cortex-M7). Teensy 3 support
was removed; do not reintroduce Kinetis/SysTick-specific code paths. The whole
library is three files: `TeensyThreads.h`, `TeensyThreads.cpp`,
`TeensyThreads-asm.S`. User-facing API documentation lives in `readme.md` and
should be updated alongside API changes.

## Building

There is no host build or unit test runner; everything targets the Teensy.
`arduino-cli` is not installed. Compile-check with PlatformIO, which has the
Teensy toolchain at `~/.platformio/packages/`. Make a scratch project (not in
this repo) with this `platformio.ini`, copy an example sketch to
`src/main.cpp` with `#include <Arduino.h>` prepended, and run `pio run`:

```ini
[env:teensy41]
platform = teensy
board = teensy41
framework = arduino
lib_deps = symlink:///home/ckidder/Arduino/libraries/TeensyThreads4
build_flags = -Wall -Wextra
```

PlatformIO compiles with `-std=gnu++17`, but the user's Arduino IDE
(Teensyduino) compiles with **gnu++14**. Code that relies on C++17 guaranteed
copy elision (e.g. copy-initializing a non-movable type such as a `Queue`)
passes under PlatformIO and fails in the IDE. To check, take the `g++` line
for `src/main.cpp` from `pio run -v`, switch it to `-std=gnu++14` and run it.
PlatformIO's newer core then errors in `IntervalTimer.h`; ignore those.

Building every example under `examples/` is the regression check. The existing
`-Wcast-function-type` warnings from the `addThread` overloads are expected.

## Testing

`examples/Tests/Tests.ino` is the test suite. It runs on hardware and prints
`Test <name> OK/***FAIL***` over Serial. It overrides the weak
`stack_overflow_isr()` to test overflow detection. Only the user can flash and
run sketches; ask them for the serial output.

## Architecture

**Scheduling.** A GPT timer (GPT1, or GPT2 if GPT1 is taken; priority 255 so
it never preempts another ISR) and the SVC handler (`yield()`) both branch
into `context_switch` in `TeensyThreads-asm.S`. The assembly saves r4-r11, lr
and the FPU registers into `currentSave`, saves PSP into `currentSP`, and calls
`loadNextThread()`, i.e. `Threads::getNextThread()`. That C++ function updates
CPU-usage accounting, checks for stack overflow, wakes `SLEEPING` (and timed
`BLOCKED`) threads, and picks the next `RUNNING` thread round-robin. It
communicates with the assembly only through the `extern "C"` globals at the
top of `TeensyThreads.cpp` (`currentThread`, `currentSave`, `currentSP`,
`currentMSP`, `currentCount`, `currentActive`). Keep those in sync when
changing either side.

**Thread 0** is the Arduino `setup()`/`loop()` context. It runs on MSP, while
all other threads run on PSP. It is the head of the thread list and is
*always* scheduled, regardless of its flags. Anything that "blocks" thread 0
(`sleep()`, queue waits, `Mutex`) must therefore poll in a loop that yields and
checks its own timeout. The assembly never saves MSP, so thread 0's stack
pointer is read with `mrs msp` instead.

**Thread list.** A singly linked list of `ThreadInfo` ordered by id. Nodes are
only appended at the tail and never freed, so the ISR can walk the list at any
time without locking. Code such as `wait()` also holds node pointers across
yields, using `generation` to detect reuse. An `ENDED`/`EMPTY` node is reused
by `addThread()`. When the library allocates a stack, `newThreadInfo()` puts
the stack and its `ThreadInfo` in one block, with the `ThreadInfo` *above* the
stack, so an overflow doesn't hit the thread's own state. That stack stays
with the node; it is reused only for requests of the same size or smaller.

**Thread states** (`Threads::RUNNING`, `SUSPENDED`, `SLEEPING`, `BLOCKED`,
`ENDED`, `EMPTY`, ...) live in `ThreadInfo::flags`. Only `RUNNING` threads are
scheduled. `restart()` and `wake()` set a thread back to `RUNNING`, so a woken
thread must re-check whatever it was waiting for.

**Critical sections.** There are two styles:
- `threads.stop()`/`start(old)`: disables scheduling only. Used by
  `addThread` and `Mutex`.
- Interrupts disabled: used where ISRs may also touch the data (sleep, queues).
  Queue code saves and restores PRIMASK (`queue_irq_save`/`queue_irq_restore`)
  so it is safe inside ISRs. Never `yield()` (SVC) with interrupts disabled or
  from an ISR: that HardFaults.

**Blocking primitive.** In the same critical section that found the thread
must wait, call `block(obj, timeout)` with interrupts off. Then re-enable
interrupts and call `waitWhileBlocked()`. The other side calls `wake(obj)`,
which wakes *all* threads blocked on `obj`. `Threads::QueueBase` (untyped ring
buffer, in the .cpp) uses the addresses of its `readers`/`writers` members as
wait objects. `Queue<T, N>` is a thin header-only typed wrapper with inline
storage, and `T` must be trivially copyable.

**Stack overflow.** Detected only at context switches: either the stack
pointer is near the bottom, or the `0xDEADDEAD` marker at the bottom has been
overwritten. The weak `stack_overflow_isr()` then marks the thread `ENDED`.
Memory below the stack can already be damaged by then. `Serial.printf()` needs
well over 1 KB, more than the 1024-byte default stack.

**Timeouts** follow the library convention: `timeout_ms == 0` means wait
forever. Non-blocking variants are separate `try*` methods.
